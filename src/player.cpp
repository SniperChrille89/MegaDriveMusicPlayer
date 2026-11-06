#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <commctrl.h>
#include <array>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <cstring>
#include <memory>
#include "engine.h"
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")

struct Song {std::wstring path, title;};
static HWND windowHandle{}, listHandle{}, statusHandle{}, profileHandle{};
static std::array<HWND,4> sliderHandles{}, percentHandles{};
static std::array<std::atomic<int>,4> volumePercent{{75,120,110,100}}; // PSG, FM, DAC, Master: user-tested defaults
static std::vector<Song> songs;
static std::mutex songsMutex;
static std::atomic<int> requested{-1}, current{-1};
static std::atomic<bool> running{true}, pauseRequested{false}, pauseState{false};
static std::atomic<bool> nextRequested{false}, prevRequested{false}, stopRequested{false};
static std::atomic<bool> combinedYamaha{false}, analogLowpass{false};
static std::atomic<int> analogStrength{45};
static HWND lowpassSlider{}, lowpassLabel{};
static std::atomic<int> selectedRegion{60}; // 60Hz default preserves earlier playback for NTSC rips
static std::atomic<int> selectedTimingProfile{0}; // preserve VGM timestamps by default
static std::atomic<int> currentFileRate{60};
static std::atomic<bool> currentFileRateDeclared{false};
static std::thread audioThread;
static const int CTRL_OPEN=101,CTRL_PLAY=102,CTRL_PREV=103,CTRL_NEXT=104,CTRL_STOP=106,CTRL_LIST=105,CTRL_PSG=201,CTRL_FM=202,CTRL_DAC=203,CTRL_MASTER=204,CTRL_PAL=301,CTRL_NTSC=302,CTRL_PROFILE=303,CTRL_COMBINED=304,CTRL_LOWPASS=305,CTRL_FILTER_STRENGTH=306;
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
  std::ifstream file(std::filesystem::path(p),std::ios::binary);if(!file){if(!tmp.empty())DeleteFileW(tmp.c_str());return false;}
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
  MegaDriveEngine engine;bool loaded=false;int pending=-1;
  int outputCount=0;
  while(running){
    engine.setRegion(selectedRegion.load());
    engine.setTimingProfile(selectedTimingProfile.load());
    engine.setYamahaCombined(combinedYamaha.load());
    engine.setAnalogLowpass(analogLowpass.load(), analogStrength.load());
    if(stopRequested.exchange(false)){
      // Stop takes precedence over queued track changes. Allow current fade to
      // finish, and leave the current selection available for Play/restart.
      requested=-1; pending=-1; nextRequested=false; prevRequested=false;
      if(loaded && !engine.isFinished()) {
        if(engine.isPaused())engine.stop();
        else engine.requestFade();
      }
      pauseState=false;
    }
    if(nextRequested.exchange(false)){int i=current.load();int n;{std::lock_guard<std::mutex> lock(songsMutex);n=(int)songs.size();}
      if(n>0)requested=(i+1+n)%n;}
    if(prevRequested.exchange(false)){int i=current.load();int n;{std::lock_guard<std::mutex> lock(songsMutex);n=(int)songs.size();}
      if(n>0)requested=(i-1+n)%n;}
    int r=requested.exchange(-1);
    if(r>=0){pending=r;
      if(loaded && !engine.isFinished() && !engine.isPaused())engine.requestFade();
      else loaded=false; // no fade needed when already stopped/paused
    }
    if(pauseRequested.exchange(false) && loaded){
      if(engine.isFinished())pending=current.load(); // Play after Stop restarts selected song
      else if(engine.isPaused())engine.resume();
      else if(!engine.isFading())engine.requestPause();
    }
    pauseState=loaded&&engine.isPaused();
    if(pending>=0 && (!loaded || engine.isFinished())){
      Song song;bool ok=false;
      {std::lock_guard<std::mutex> lock(songsMutex);if(pending<(int)songs.size()){song=songs[pending];ok=true;}}
      if(ok){std::vector<uint8_t> bytes;std::string error;
        if(readFile(song.path,bytes) && engine.load(bytes,error)){
          engine.setRegion(selectedRegion.load());
          engine.setTimingProfile(selectedTimingProfile.load());
          currentFileRate=engine.sourceRate();currentFileRateDeclared=engine.sourceRateDeclared();
          loaded=true;current=pending;PostMessageW(windowHandle,WM_STATUS,1,0);}
        else {loaded=false;PostMessageW(windowHandle,WM_STATUS,3,0);}
      }
      pending=-1;
    }
    bool didWork=false;
    for(auto& b:buffers){
      if(!(b.header.dwFlags & WHDR_DONE))continue;
      if(loaded && !engine.isFinished()){
        engine.setVolumes(volumePercent[0].load(),volumePercent[1].load(),
                          volumePercent[2].load(),volumePercent[3].load());
        engine.render(b.data,kFrames);
      }
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
      INITCOMMONCONTROLSEX ic{};ic.dwSize=sizeof(ic);ic.dwICC=ICC_BAR_CLASSES;InitCommonControlsEx(&ic);
      CreateWindowW(L"STATIC",L"Region:",WS_CHILD|WS_VISIBLE,12,17,59,21,hwnd,nullptr,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L"PAL 50 Hz",WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON|WS_GROUP,
                    76,12,103,28,hwnd,(HMENU)CTRL_PAL,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L"NTSC 60 Hz",WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON,
                    183,12,113,28,hwnd,(HMENU)CTRL_NTSC,nullptr,nullptr);
      SendMessageW(GetDlgItem(hwnd,CTRL_NTSC),BM_SETCHECK,BST_CHECKED,0);
      CreateWindowW(L"STATIC",L"Music timing:",WS_CHILD|WS_VISIBLE,12,57,95,22,hwnd,nullptr,nullptr,nullptr);
      profileHandle=CreateWindowW(WC_COMBOBOXW,L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
                          110,51,457,180,hwnd,(HMENU)CTRL_PROFILE,nullptr,nullptr);
      SendMessageW(profileHandle,CB_ADDSTRING,0,(LPARAM)L"Original VGM timing (all games)");
      SendMessageW(profileHandle,CB_ADDSTRING,0,(LPARAM)L"Sonic 1 - PAL 50Hz VBlank slowdown");
      SendMessageW(profileHandle,CB_ADDSTRING,0,(LPARAM)L"Sonic 3 & Knuckles - PAL compensated (~97.2%)");
      SendMessageW(profileHandle,CB_SETCURSEL,0,0);
      listHandle=CreateWindowExW(WS_EX_CLIENTEDGE,L"LISTBOX",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|LBS_NOTIFY,
        12,105,555,251,hwnd,(HMENU)CTRL_LIST,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L"Open VGM / VGZ",WS_CHILD|WS_VISIBLE,415,10,152,31,hwnd,(HMENU)CTRL_OPEN,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L"<<",WS_CHILD|WS_VISIBLE,70,368,90,36,hwnd,(HMENU)CTRL_PREV,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L"Play / Pause",WS_CHILD|WS_VISIBLE,171,368,135,36,hwnd,(HMENU)CTRL_PLAY,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L"Stop",WS_CHILD|WS_VISIBLE,317,368,85,36,hwnd,(HMENU)CTRL_STOP,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L">>",WS_CHILD|WS_VISIBLE,413,368,90,36,hwnd,(HMENU)CTRL_NEXT,nullptr,nullptr);
      statusHandle=CreateWindowW(L"STATIC",L"Select PAL or NTSC, then open a VGM/VGZ",WS_CHILD|WS_VISIBLE,
        12,83,555,20,hwnd,nullptr,nullptr,nullptr);
      // Four independent mix buses; 100% is the original v0.4 mix level.
      const wchar_t* names[]={L"PSG",L"FM",L"DAC",L"Master"};
      const int ids[]={CTRL_PSG,CTRL_FM,CTRL_DAC,CTRL_MASTER};
      for(int i=0;i<4;i++){
        const int y=420+i*43;
        CreateWindowW(L"STATIC",names[i],WS_CHILD|WS_VISIBLE,14,y+6,75,24,
                      hwnd,nullptr,nullptr,nullptr);
        sliderHandles[i]=CreateWindowExW(0,TRACKBAR_CLASSW,L"",WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS,
                              96,y,385,32,hwnd,(HMENU)(INT_PTR)ids[i],nullptr,nullptr);
        SendMessageW(sliderHandles[i],TBM_SETRANGE,TRUE,MAKELPARAM(0,200));
        SendMessageW(sliderHandles[i],TBM_SETPOS,TRUE,volumePercent[i].load());
        percentHandles[i]=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_RIGHT,
                                  489,y+5,72,25,hwnd,nullptr,nullptr,nullptr);
        std::wstring value=std::to_wstring(volumePercent[i].load())+L" %";
        SetWindowTextW(percentHandles[i],value.c_str());
      }
      CreateWindowW(L"BUTTON",L"Yamaha combined output (diagnostic: DAC follows FM)",
                    WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,14,599,460,25,hwnd,
                    (HMENU)CTRL_COMBINED,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L"Optional low-pass filter",
                    WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,14,631,210,25,hwnd,
                    (HMENU)CTRL_LOWPASS,nullptr,nullptr);
      lowpassSlider=CreateWindowExW(0,TRACKBAR_CLASSW,L"",WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS,
                    227,629,250,30,hwnd,(HMENU)CTRL_FILTER_STRENGTH,nullptr,nullptr);
      SendMessageW(lowpassSlider,TBM_SETRANGE,TRUE,MAKELPARAM(5,95));
      SendMessageW(lowpassSlider,TBM_SETPOS,TRUE,45);
      lowpassLabel=CreateWindowW(L"STATIC",L"45 %",WS_CHILD|WS_VISIBLE,484,632,75,24,hwnd,nullptr,nullptr,nullptr);
      HFONT font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
      for(HWND h: {listHandle,statusHandle,GetDlgItem(hwnd,CTRL_OPEN),GetDlgItem(hwnd,CTRL_PREV),GetDlgItem(hwnd,CTRL_PLAY),GetDlgItem(hwnd,CTRL_STOP),GetDlgItem(hwnd,CTRL_NEXT)})
        SendMessageW(h,WM_SETFONT,(WPARAM)font,TRUE);
      for(HWND h:percentHandles)SendMessageW(h,WM_SETFONT,(WPARAM)font,TRUE);
      SendMessageW(profileHandle,WM_SETFONT,(WPARAM)font,TRUE);
      for(HWND h: {GetDlgItem(hwnd,CTRL_PAL),GetDlgItem(hwnd,CTRL_NTSC)})
        SendMessageW(h,WM_SETFONT,(WPARAM)font,TRUE);

      return 0;
    }
    case WM_HSCROLL:{
      HWND source=(HWND)l;
      for(int i=0;i<4;i++)if(source==sliderHandles[i]){
        const int value=(int)SendMessageW(source,TBM_GETPOS,0,0);
        volumePercent[i].store(value);
        const std::wstring label=std::to_wstring(value)+L" %";
        SetWindowTextW(percentHandles[i],label.c_str());
        break;
      }
      if(source==lowpassSlider){
        int value=(int)SendMessageW(lowpassSlider,TBM_GETPOS,0,0);
        analogStrength=value;
        std::wstring label=std::to_wstring(value)+L" %";
        SetWindowTextW(lowpassLabel,label.c_str());
      }
      return 0;
    }
    case WM_COMMAND:{
      int id=LOWORD(w);
      if(id==CTRL_COMBINED)combinedYamaha=(SendMessageW(GetDlgItem(hwnd,CTRL_COMBINED),BM_GETCHECK,0,0)==BST_CHECKED);
      if(id==CTRL_LOWPASS)analogLowpass=(SendMessageW(GetDlgItem(hwnd,CTRL_LOWPASS),BM_GETCHECK,0,0)==BST_CHECKED);
      if(id==CTRL_OPEN)openSong();
      if(id==CTRL_PROFILE && HIWORD(w)==CBN_SELCHANGE){
        const int profile=(int)SendMessageW(profileHandle,CB_GETCURSEL,0,0);
        if(profile>=0 && profile<=2)selectedTimingProfile=profile;
        PostMessageW(windowHandle,WM_STATUS,1,0);
      }
      if(id==CTRL_PAL || id==CTRL_NTSC){
        selectedRegion=(id==CTRL_PAL ? 50 : 60);
        PostMessageW(windowHandle,WM_STATUS,1,0);
      }
      if(id==CTRL_PLAY)pauseRequested=true;
      if(id==CTRL_STOP)stopRequested=true;
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
          const std::wstring region=selectedRegion.load()==50?L"PAL 50Hz":L"NTSC 60Hz";
          const std::wstring source=currentFileRateDeclared.load()?
              (L"source "+std::to_wstring(currentFileRate.load())+L"Hz"):
              L"source unspecified (60Hz assumed)";
          const int profile=selectedTimingProfile.load();
          const std::wstring timing=profile==0?L"VGM original timing":
              (profile==1?L"Sonic 1 timing":L"Sonic 3K timing (approx.)");
          showStatus(region+L" | "+timing+L" | "+source+L" | "+title);
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
    CW_USEDEFAULT,CW_USEDEFAULT,595,720,nullptr,nullptr,inst,nullptr);
  if(!windowHandle)return 2;
  ShowWindow(windowHandle,show);UpdateWindow(windowHandle);
  audioThread=std::thread(audioWorker);
  MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
  running=false;if(audioThread.joinable())audioThread.join();return 0;
}
