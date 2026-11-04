#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <fstream>
#include <iterator>
#include <memory>
#include "engine.h"
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "comdlg32.lib")

struct Song {std::wstring path, title;};
static HWND windowHandle{}, listHandle{}, statusHandle{};
static std::vector<Song> songs;
static std::mutex songsMutex;
static std::atomic<int> requested{-1}, current{-1};
static std::atomic<bool> running{true}, pauseRequested{false}, pauseState{false};
static std::atomic<bool> nextRequested{false}, prevRequested{false};
static std::thread audioThread;
static const int CTRL_OPEN=101,CTRL_PLAY=102,CTRL_PREV=103,CTRL_NEXT=104,CTRL_LIST=105;
static const UINT WM_STATUS=WM_APP+10;

static std::wstring appDirectory(){
  wchar_t buf[MAX_PATH]{}; GetModuleFileNameW(nullptr,buf,MAX_PATH);
  std::wstring out(buf); auto pos=out.find_last_of(L"\\/");return pos==std::wstring::npos?L".":out.substr(0,pos);
}
static std::wstring quote(const std::wstring& s){return L"\""+s+L"\"";}
static bool decompressVGZ(const std::wstring& filename,std::wstring& out){
  wchar_t tempDir[MAX_PATH]{},tempName[MAX_PATH]{};
  if(!GetTempPathW(MAX_PATH,tempDir) || !GetTempFileNameW(tempDir,L"vgm",0,tempName))return false;
  out=tempName;
  const auto script=appDirectory()+L"\\unvgz.ps1";
  std::wstring command=L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File "+quote(script)+L" "+quote(filename)+L" "+quote(out);
  STARTUPINFOW si{};si.cb=sizeof si;PROCESS_INFORMATION pi{};
  if(!CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)){DeleteFileW(out.c_str());return false;}
  WaitForSingleObject(pi.hProcess,INFINITE);DWORD exitCode=1;GetExitCodeProcess(pi.hProcess,&exitCode);
  CloseHandle(pi.hProcess);CloseHandle(pi.hThread);
  if(exitCode){DeleteFileW(out.c_str());return false;}return true;
}
static bool readFile(const std::wstring& filename,std::vector<uint8_t>& bytes){
  std::wstring p=filename,tmp;
  if(p.size()>=4 && _wcsicmp(p.c_str()+p.size()-4,L".vgz")==0){if(!decompressVGZ(p,tmp))return false;p=tmp;}
  std::ifstream file(p,std::ios::binary);if(!file){if(!tmp.empty())DeleteFileW(tmp.c_str());return false;}
  bytes.assign(std::istreambuf_iterator<char>(file),{});
  if(!tmp.empty())DeleteFileW(tmp.c_str());return !bytes.empty();
}
static void showStatus(const std::wstring& str){SetWindowTextW(statusHandle,str.c_str());}
static void requestSong(int i){ if(i>=0){requested=i;pauseRequested=false;PostMessageW(windowHandle,WM_STATUS,0,0);} }
static void openSong(){
  wchar_t filename[32768]{};
  OPENFILENAMEW ofn{};ofn.lStructSize=sizeof ofn;ofn.hwndOwner=windowHandle;
  ofn.lpstrFilter=L"VGM / VGZ files\0*.vgm;*.vgz\0All files\0*.*\0\0";
  ofn.lpstrFile=filename;ofn.nMaxFile=32768;
  ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY;
  if(!GetOpenFileNameW(&ofn))return;
  std::wstring path(filename);auto p=path.find_last_of(L"\\/");
  std::wstring title=p==std::wstring::npos?path:path.substr(p+1);
  int idx;
  {std::lock_guard<std::mutex> lock(songsMutex);idx=(int)songs.size();songs.push_back({path,title});}
  SendMessageW(listHandle,LB_ADDSTRING,0,(LPARAM)title.c_str());
  SendMessageW(listHandle,LB_SETCURSEL,idx,0);
  requestSong(idx);
}
static void audioWorker(){
  WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=2;fmt.nSamplesPerSec=MegaDriveEngine::Rate;
  fmt.wBitsPerSample=16;fmt.nBlockAlign=4;fmt.nAvgBytesPerSec=fmt.nSamplesPerSec*4;
  HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);HWAVEOUT wo{};
  MMRESULT ret=waveOutOpen(&wo,WAVE_MAPPER,&fmt,(DWORD_PTR)event,0,CALLBACK_EVENT);
  if(ret!=MMSYSERR_NOERROR){PostMessageW(windowHandle,WM_STATUS,2,0);CloseHandle(event);return;}
  constexpr int kBuffers=4,kFrames=1024;
  struct Buffer{WAVEHDR header{};int16_t data[kFrames*2]{};};
  Buffer buffers[kBuffers]{};
  for(auto& b:buffers){b.header.lpData=(LPSTR)b.data;b.header.dwBufferLength=sizeof b.data;
    waveOutPrepareHeader(wo,&b.header,sizeof(WAVEHDR));b.header.dwFlags|=WHDR_DONE;}
  MegaDriveEngine engine;bool loaded=false;int pending=-1;bool fadingToNext=false;
  int outputCount=0;
  while(running){
    if(nextRequested.exchange(false)){int i=current.load();int n;{std::lock_guard<std::mutex> lock(songsMutex);n=(int)songs.size();}
      if(n>0)requested=(i+1+n)%n;}
    if(prevRequested.exchange(false)){int i=current.load();int n;{std::lock_guard<std::mutex> lock(songsMutex);n=(int)songs.size();}
      if(n>0)requested=(i-1+n)%n;}
    int r=requested.exchange(-1);
    if(r>=0 && r!=current){pending=r;
      if(loaded && !engine.isFinished() && !engine.isPaused()){engine.requestFade();fadingToNext=true;}
      else {fadingToNext=false;loaded=false;}
    }
    if(pauseRequested.exchange(false) && loaded){if(engine.isPaused())engine.resume();else engine.requestPause();}
    pauseState=loaded&&engine.isPaused();
    if(pending>=0 && (!loaded || engine.isFinished())){
      Song song;bool ok=false;
      {std::lock_guard<std::mutex> lock(songsMutex);if(pending<(int)songs.size()){song=songs[pending];ok=true;}}
      if(ok){std::vector<uint8_t> bytes;std::string error;
        if(readFile(song.path,bytes) && engine.load(bytes,error)){loaded=true;current=pending;PostMessageW(windowHandle,WM_STATUS,1,0);}
        else {loaded=false;PostMessageW(windowHandle,WM_STATUS,3,0);}
      }
      pending=-1;fadingToNext=false;
    }
    bool didWork=false;
    for(auto& b:buffers){
      if(!(b.header.dwFlags & WHDR_DONE))continue;
      if(loaded && !engine.isFinished())engine.render(b.data,kFrames);
      else memset(b.data,0,sizeof b.data);
      b.header.dwBufferLength=sizeof b.data;
      b.header.dwFlags&=~WHDR_DONE;
      if(waveOutWrite(wo,&b.header,sizeof(WAVEHDR))==MMSYSERR_NOERROR){didWork=true;++outputCount;}
      else b.header.dwFlags|=WHDR_DONE;
    }
    if(!didWork)WaitForSingleObject(event,10);
  }
  waveOutReset(wo);
  for(auto& b:buffers)waveOutUnprepareHeader(wo,&b.header,sizeof(WAVEHDR));
  waveOutClose(wo);CloseHandle(event);
}
static LRESULT CALLBACK wndProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
  switch(msg){
    case WM_CREATE:{
      listHandle=CreateWindowExW(WS_EX_CLIENTEDGE,L"LISTBOX",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|LBS_NOTIFY,
        12,48,555,280,hwnd,(HMENU)CTRL_LIST,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L"Open VGM / VGZ",WS_CHILD|WS_VISIBLE,12,9,135,31,hwnd,(HMENU)CTRL_OPEN,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L"<<",WS_CHILD|WS_VISIBLE,95,340,90,36,hwnd,(HMENU)CTRL_PREV,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L"Play / Pause",WS_CHILD|WS_VISIBLE,201,340,168,36,hwnd,(HMENU)CTRL_PLAY,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L">>",WS_CHILD|WS_VISIBLE,384,340,90,36,hwnd,(HMENU)CTRL_NEXT,nullptr,nullptr);
      statusHandle=CreateWindowW(L"STATIC",L"Open a .vgm or .vgz to begin · Nuked YM2612 + PSG",WS_CHILD|WS_VISIBLE,
        156,16,410,22,hwnd,nullptr,nullptr,nullptr);
      HFONT font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
      for(HWND h: {listHandle,statusHandle,GetDlgItem(hwnd,CTRL_OPEN),GetDlgItem(hwnd,CTRL_PREV),GetDlgItem(hwnd,CTRL_PLAY),GetDlgItem(hwnd,CTRL_NEXT)})
        SendMessageW(h,WM_SETFONT,(WPARAM)font,TRUE);
      return 0;
    }
    case WM_COMMAND:{
      int id=LOWORD(w);
      if(id==CTRL_OPEN)openSong();
      if(id==CTRL_PLAY)pauseRequested=true;
      if(id==CTRL_PREV)prevRequested=true;
      if(id==CTRL_NEXT)nextRequested=true;
      if(id==CTRL_LIST && HIWORD(w)==LBN_DBLCLK)requestSong((int)SendMessageW(listHandle,LB_GETCURSEL,0,0));
      return 0;
    }
    case WM_STATUS:{
      if(w==2)showStatus(L"Could not open Windows audio output");
      else if(w==3)showStatus(L"Could not load file (unsupported VGM/VGZ or decoder failure)");
      else {
        int i=current.load();std::wstring title;
        {std::lock_guard<std::mutex> lock(songsMutex);if(i>=0 && i<(int)songs.size())title=songs[i].title;}
        if(!title.empty()){
          showStatus(L"Playing: "+title);
          SendMessageW(listHandle,LB_SETCURSEL,i,0);
        }
      }
      return 0;
    }
    case WM_DESTROY:running=false;PostQuitMessage(0);return 0;
  }
  return DefWindowProcW(hwnd,msg,w,l);
}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,LPWSTR,int show){
  WNDCLASSW wc{};wc.lpfnWndProc=wndProc;wc.hInstance=inst;wc.lpszClassName=L"MegaDriveAudioPlayerClass";
  wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
  if(!RegisterClassW(&wc))return 1;
  windowHandle=CreateWindowExW(0,wc.lpszClassName,L"Mega Drive Audio — Desktop Test",WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT,CW_USEDEFAULT,595,440,nullptr,nullptr,inst,nullptr);
  if(!windowHandle)return 2;
  ShowWindow(windowHandle,show);UpdateWindow(windowHandle);
  audioThread=std::thread(audioWorker);
  MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
  running=false;if(audioThread.joinable())audioThread.join();return 0;
}
