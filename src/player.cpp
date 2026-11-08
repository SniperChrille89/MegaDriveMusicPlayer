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
#include <future>
#include <chrono>
#include <algorithm>
#include <map>
#include <cmath>
#ifdef MDA_ROM_AUDIO_ENABLED
#include "hardware/rom_audio.h"
#endif
#include "engine.h"
#include "hardware/rom_catalog.h"
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")

struct Song {std::wstring path, title; bool fromRom=false; uint8_t musicId=0;};
static HWND windowHandle{}, listHandle{}, statusHandle{}, profileHandle{};
static mda::RomCatalog openedCatalog;
static std::vector<mda::RomTrack> romTracks;
static bool romListMode=false;
static std::atomic<bool> romModeActive{false};
static std::shared_ptr<const std::vector<uint8_t>> openedRom;
static std::mutex romMutex;
static std::wstring romTitle;
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
static const int CTRL_OPEN=101,CTRL_PLAY=102,CTRL_PREV=103,CTRL_NEXT=104,CTRL_STOP=106,CTRL_LIST=105,CTRL_PSG=201,CTRL_FM=202,CTRL_DAC=203,CTRL_MASTER=204,CTRL_PAL=301,CTRL_NTSC=302,CTRL_PROFILE=303,CTRL_COMBINED=304,CTRL_LOWPASS=305,CTRL_FILTER_STRENGTH=306,CTRL_OPEN_ROM=307,CTRL_ROM_LIST=308;
static const UINT WM_STATUS=WM_APP+10;
// Timeline intentionally removed. ROM playback continues without a seek bar.
static std::atomic<uint64_t> playPosition{0}, playDuration{0}, playLoopStart{UINT64_MAX};
static std::atomic<uint64_t> seekRequested{UINT64_MAX};
static std::atomic<bool> seekBusy{false};
static uint32_t readLe32(const std::vector<uint8_t>& b,size_t off) {
  if(off+4>b.size())return 0;
  return uint32_t(b[off])|(uint32_t(b[off+1])<<8)|(uint32_t(b[off+2])<<16)|(uint32_t(b[off+3])<<24);
}
// VGM's loop offset points to a COMMAND, not a timestamp. Sum preceding waits.
// A ROM-generated trace has no loop offset and is deliberately reported as unknown.
static uint64_t vgmLoopSample(const std::vector<uint8_t>& b) {
  if(b.size()<0x40 || std::memcmp(b.data(),"Vgm ",4)!=0)return UINT64_MAX;
  const uint32_t off=readLe32(b,0x1c);
  if(!off)return UINT64_MAX;
  const uint64_t end=uint64_t(0x1c)+off;
  size_t p=readLe32(b,8)>=0x150?(readLe32(b,0x34)?size_t(0x34)+readLe32(b,0x34):0x40):0x40;
  if(end>=b.size() || end<p)return UINT64_MAX;
  uint64_t samples=0;
  while(p<end){
    const uint8_t op=b[p++];
    if(op==0x61){if(p+2>b.size())break;samples+=b[p]|(uint16_t(b[p+1])<<8);p+=2;}
    else if(op==0x62)samples+=735;
    else if(op==0x63)samples+=882;
    else if(op>=0x70&&op<=0x7f)samples+=(op&15)+1;
    else if(op>=0x80&&op<=0x8f)samples+=(op&15);
    else if(op==0x50 || op==0x4f || (op>=0x30&&op<=0x3f))p+=1;
    else if((op>=0x51&&op<=0x5f)||(op>=0xa0&&op<=0xbf))p+=2;
    else if(op>=0xc0&&op<=0xdf)p+=3;
    else if(op==0x67){if(p+6>b.size() || b[p]!=0x66)break;p+=2;const auto len=readLe32(b,p);p+=4+len;}
    else if(op==0xe0)p+=4;
    else if(op==0x66)break;
    else break;
    if(p>b.size())break;
  }
  return p==end?samples:UINT64_MAX;
}
static std::wstring clockText(uint64_t sample){
  const uint64_t sec=sample/MegaDriveEngine::Rate;
  wchar_t buf[48]{};swprintf_s(buf,L"%02llu:%02llu",(unsigned long long)(sec/60),(unsigned long long)(sec%60));
  return buf;
}
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
  if(romListMode){
    romListMode=false;romTracks.clear();
    SendMessageW(listHandle,LB_RESETCONTENT,0,0);
    {std::lock_guard<std::mutex> lock(songsMutex);songs.clear();}
    current=-1;
  }
  std::wstring path(filename);auto p=path.find_last_of(L"\\/");
  std::wstring title=p==std::wstring::npos?path:path.substr(p+1);
  int idx;
  {std::lock_guard<std::mutex> lock(songsMutex);idx=(int)songs.size();songs.push_back({path,title});}
  SendMessageW(listHandle,LB_ADDSTRING,0,(LPARAM)title.c_str());
  SendMessageW(listHandle,LB_SETCURSEL,idx,0);
  requestSong(idx);
}
static std::wstring widenAscii(const std::string& text){
    return std::wstring(text.begin(),text.end());
}
static void openRom(){
    wchar_t filename[32768]{};
    OPENFILENAMEW ofn{};ofn.lStructSize=sizeof ofn;ofn.hwndOwner=windowHandle;
    ofn.lpstrFilter=L"Mega Drive ROM (*.bin;*.md)\0*.bin;*.md\0All files\0*.*\0\0";
    ofn.lpstrFile=filename;ofn.nMaxFile=32768;
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY;
    if(!GetOpenFileNameW(&ofn))return;
    std::ifstream file(std::filesystem::path(filename),std::ios::binary);
    std::vector<uint8_t> bytes;
    if(file)bytes.assign(std::istreambuf_iterator<char>(file),{});
    auto inspected=mda::inspectRom(bytes);
    if(!inspected.valid){
        std::wstring err=L"ROM could not be recognized: "+widenAscii(inspected.error);
        MessageBoxW(windowHandle,err.c_str(),L"Invalid ROM",MB_OK|MB_ICONWARNING);return;
    }
    // The emulator-driver path isn't connected to audio output yet. Make the
    // selection visible in the existing playlist, without pretending it plays.
    stopRequested=true;
    requested=-1;
    pauseRequested=false;
    romListMode=true;
    romModeActive=true;
    {std::lock_guard<std::mutex> lock(romMutex);openedRom=std::make_shared<const std::vector<uint8_t>>(std::move(bytes));}
    openedCatalog=std::move(inspected);
    romTracks=openedCatalog.tracks;
    romTitle=widenAscii(openedCatalog.overseasTitle);
    {
        std::lock_guard<std::mutex> lock(songsMutex);
        songs.clear();
    }
    current=-1;
    SendMessageW(listHandle,LB_RESETCONTENT,0,0);
    for(const auto& track:romTracks){
        const std::wstring label=widenAscii(track.label);
        {std::lock_guard<std::mutex> lock(songsMutex);songs.push_back({L"",label,true,track.command});}
        SendMessageW(listHandle,LB_ADDSTRING,0,(LPARAM)label.c_str());
    }
    if(!romTracks.empty())SendMessageW(listHandle,LB_SETCURSEL,0,0);
#ifdef MDA_ROM_AUDIO_ENABLED
    showStatus(L"ROM: "+romTitle+L" | Select song and press Play (Z80 rendering in memory)");
#else
    showStatus(L"ROM: "+romTitle+L" | ROM audio disabled in this build");
#endif
    SetWindowTextW(GetDlgItem(windowHandle,CTRL_OPEN_ROM),L"Open ROM");
}
static void showRomPending(){
    const int i=(int)SendMessageW(listHandle,LB_GETCURSEL,0,0);
    if(i<0 || i >= (int)romTracks.size())return;
    std::wstring msg=L"Selected "+widenAscii(romTracks[i].label)+
        L" from "+romTitle+L".\n\n"
        L"This is a catalog entry, NOT playable ROM audio yet. "
        L"The original Sonic 3K Z80 driver still needs bootstrapping, "
        L"music command dispatch, IRQ timing and PCM output wiring.\n\n"
        L"You can still open VGM/VGZ files with the existing player.";
    MessageBoxW(windowHandle,msg.c_str(),L"ROM music playback – not yet implemented",MB_OK|MB_ICONINFORMATION);
}
static void audioWorker(){
  WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=2;fmt.nSamplesPerSec=MegaDriveEngine::Rate;
  fmt.wBitsPerSample=16;fmt.nBlockAlign=4;fmt.nAvgBytesPerSec=fmt.nSamplesPerSec*4;
  HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);HWAVEOUT wo{};
  MMRESULT ret=waveOutOpen(&wo,WAVE_MAPPER,&fmt,(DWORD_PTR)event,0,CALLBACK_EVENT);
  if(ret!=MMSYSERR_NOERROR){PostMessageW(windowHandle,WM_STATUS,2,0);CloseHandle(event);return;}
  constexpr int kBuffers=4,kFrames=512; // ~46 ms total waveOut queue at 44.1 kHz
  struct Buffer{WAVEHDR header{};int16_t data[kFrames*2]{};};
  Buffer buffers[kBuffers]{};
  for(auto& b:buffers){b.header.lpData=(LPSTR)b.data;b.header.dwBufferLength=sizeof b.data;
    waveOutPrepareHeader(wo,&b.header,sizeof(WAVEHDR));b.header.dwFlags|=WHDR_DONE;}
  MegaDriveEngine engine;bool loaded=false;int pending=-1;
#ifdef MDA_ROM_AUDIO_ENABLED
  std::future<mda::RomAudioResult> romJob;
  bool romJobRunning=false;
  std::shared_ptr<mda::Sonic3KStream> romStream;
  std::future<mda::RomAudioChunk> extendJob;
  bool extending=false;
  uint64_t streamTotalSamples=0;
  bool activeRomStream=false;
  uint64_t streamEpoch=0,extendEpoch=0;
  // Z80 runs in single video frames. Maintain ~60 ms of future ROM data,
  // not whole seconds. Windows keeps only ~46 ms of queued PCM.
  constexpr uint64_t AheadThreshold=MegaDriveEngine::Rate*3/50;
  constexpr unsigned InitialFrames=4;
  constexpr unsigned RefillFrames=1;
  bool romFadePending=false,romFadeComplete=false;
  uint64_t romFadeEnd=0;

  int romJobIndex=-1;
  int romJobRegion=50;
#endif
  // Checkpoints preserve the complete YM/PSG/PCM state and make seeking
  // back to already-played segments fast without approximating chip envelopes.
  std::map<uint64_t,MegaDriveEngine> checkpoints;
  std::vector<uint8_t> loadedVgm;
  uint64_t lastCheckpoint=0;
  int outputCount=0;
  while(running){
    engine.setRegion(selectedRegion.load());
    engine.setTimingProfile(selectedTimingProfile.load());
    engine.setYamahaCombined(combinedYamaha.load());
    engine.setAnalogLowpass(analogLowpass.load(), analogStrength.load());
    if(stopRequested.exchange(false)){
      requested=-1;pending=-1;nextRequested=false;prevRequested=false;
#ifdef MDA_ROM_AUDIO_ENABLED
      if(activeRomStream && romStream && loaded && !engine.isFinished()) {
        // The fade is generated INSIDE the original Sonic 3K sound driver.
        // No master fade, no FM TL rewriting, and no immediate PSG/DAC mute.
        romStream->cancelQueuedSong();
        romStream->sendCommand(0xE1);
        romFadePending=true;romFadeComplete=false;
      } else
#endif
      if(loaded && !engine.isFinished()) {
        if(engine.isPaused())engine.stop();else engine.requestFade();
      }
      pauseState=false;
    }
    if(nextRequested.exchange(false)){int i=current.load();int n;{std::lock_guard<std::mutex> lock(songsMutex);n=(int)songs.size();}
      if(n>0)requested=(i+1+n)%n;}
    if(prevRequested.exchange(false)){int i=current.load();int n;{std::lock_guard<std::mutex> lock(songsMutex);n=(int)songs.size();}
      if(n>0)requested=(i-1+n)%n;}
    int r=requested.exchange(-1);
    if(r>=0){pending=r;
#ifdef MDA_ROM_AUDIO_ENABLED
      if(activeRomStream && romStream && loaded && !engine.isFinished()){
        Song target;bool valid=false;
        {std::lock_guard<std::mutex> lock(songsMutex);
          if(r<(int)songs.size()){target=songs[r];valid=true;}}
        if(valid && target.fromRom && target.musicId) {
          // Do not tear down Z80 / YM2612 / PSG between Sonic tracks.
          // Queue the next music ID so the original driver receives it near
          // the end of its own fade, with no silent device reset in between.
          romStream->queueSongAfterFade(target.musicId);
          if(!romFadePending)romStream->sendCommand(0xE1);
          romFadePending=true;romFadeComplete=false;
        } else {
          romStream->cancelQueuedSong();
          romStream->sendCommand(0xE1);
          romFadePending=true;romFadeComplete=false;
        }
      } else
#endif
      if(loaded && !engine.isFinished() && !engine.isPaused())engine.requestFade();
      else loaded=false;
    }
    if(pauseRequested.exchange(false) && loaded){
      if(engine.isFinished())pending=current.load(); // Play after Stop restarts selected song
      else if(engine.isPaused())engine.resume();
      else if(!engine.isFading())engine.requestPause();
    }
    pauseState=loaded&&engine.isPaused();
#ifdef MDA_ROM_AUDIO_ENABLED
    if(extending && extendJob.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
      auto chunk=extendJob.get();extending=false;
      if(activeRomStream && extendEpoch==streamEpoch && romModeActive.load() && chunk.error.empty() && !chunk.vgm.empty()) {
        if(engine.appendRomCommands(chunk.vgm,chunk.totalSamples)){
          streamTotalSamples=chunk.totalSamples;
          // This is a buffer horizon, NOT a musical loop boundary.
          playDuration=streamTotalSamples;
          if(chunk.songSwitched && pending>=0){
            // The next song has begun INSIDE the live Z80 session, and all
            // corresponding chip writes are already in this audio trace.
            current=pending;pending=-1;
            romFadePending=false;romFadeComplete=false;romFadeEnd=0;
            PostMessageW(windowHandle,WM_STATUS,4,0);
          } else if(romFadePending && chunk.fadeFinished){
            romFadeComplete=true;romFadeEnd=chunk.totalSamples;
          }
        }
      }
    }
    if(romJobRunning && romJob.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
      auto result=romJob.get();romJobRunning=false;
      // Discard a stale result when a different song/mode was requested.
      if(romJobIndex==pending && romModeActive.load() && !result.vgm.empty()){
        std::string error;
        if(engine.load(result.vgm,error)){
          engine.setRegion(romJobRegion);engine.setTimingProfile(0);
          engine.enableRomStream();activeRomStream=true;++streamEpoch;
          romFadePending=false;romFadeComplete=false;romFadeEnd=0;
          streamTotalSamples=result.totalSamples;
          currentFileRate=romJobRegion;currentFileRateDeclared=true;
          loaded=true;current=pending;pending=-1;
          loadedVgm=result.vgm;checkpoints.clear();checkpoints.emplace(0,engine);
          lastCheckpoint=0;playPosition=0;
          playDuration=result.totalSamples;
          playLoopStart=UINT64_MAX;
          PostMessageW(windowHandle,WM_STATUS,4,0);
        }else {loaded=false;pending=-1;PostMessageW(windowHandle,WM_STATUS,3,0);}
      }else if(romJobIndex==pending && romModeActive.load()){
        loaded=false;pending=-1;PostMessageW(windowHandle,WM_STATUS,5,0);
      }
    }
#endif
#ifdef MDA_ROM_AUDIO_ENABLED
    // Wait until the original driver reports its fade finished and its
    // remaining YM/PSG commands have actually been rendered.
    if(romFadeComplete && engine.position()>=romFadeEnd){
      engine.stop();loaded=false;activeRomStream=false;
      romFadePending=false;romFadeComplete=false;++streamEpoch;
    }
#endif
    if(pending>=0 && (!loaded || engine.isFinished())){
      Song song;bool ok=false;
      {std::lock_guard<std::mutex> lock(songsMutex);if(pending<(int)songs.size()){song=songs[pending];ok=true;}}
      if(ok){
#ifdef MDA_ROM_AUDIO_ENABLED
        if(song.fromRom){
          if(!romJobRunning){
            std::shared_ptr<const std::vector<uint8_t>> rom;
            {std::lock_guard<std::mutex> lock(romMutex);rom=openedRom;}
            if(rom){
              romJobIndex=pending;romJobRegion=selectedRegion.load();
              auto id=song.musicId;auto hz=romJobRegion;
              // A fresh persistent CPU is created for each song, and reused
              // by the asynchronous extension tasks.
              try {
                romStream=std::make_shared<mda::Sonic3KStream>(*rom,id,hz);
                auto session=romStream;
                romJob=std::async(std::launch::async,[session, frames=InitialFrames]{
                    auto chunk=session->advanceFrames(frames);
                    mda::RomAudioResult out;
                    out.vgm=std::move(chunk.vgm);out.error=std::move(chunk.error);
                    out.totalSamples=chunk.totalSamples;
                    return out;
                });
              } catch(const std::exception&) {pending=-1;PostMessageW(windowHandle,WM_STATUS,5,0);continue;}
              romJobRunning=true;
              PostMessageW(windowHandle,WM_STATUS,6,0);
            }else {pending=-1;PostMessageW(windowHandle,WM_STATUS,5,0);}
          }
        } else
#endif
        {std::vector<uint8_t> bytes;std::string error;
        if(readFile(song.path,bytes) && engine.load(bytes,error)){
          engine.setRegion(selectedRegion.load());
          engine.setTimingProfile(selectedTimingProfile.load());
          currentFileRate=engine.sourceRate();currentFileRateDeclared=engine.sourceRateDeclared();
          loaded=true;current=pending;
          loadedVgm=bytes;checkpoints.clear();checkpoints.emplace(0,engine);
          lastCheckpoint=0;playPosition=0;
          playDuration=uint64_t(readLe32(bytes,0x18));
          playLoopStart=vgmLoopSample(bytes);
          PostMessageW(windowHandle,WM_STATUS,1,0);}
        else {loaded=false;PostMessageW(windowHandle,WM_STATUS,3,0);}
        pending=-1;
        }
      }
      else pending=-1;
    }
#ifdef MDA_ROM_AUDIO_ENABLED
    // Render more original-driver events while the previous audio is playing.
    // Never reset the emulated chips at the chunk boundary.
    if(activeRomStream && !extending && !romJobRunning && romStream &&
       streamTotalSamples>engine.position() &&
       streamTotalSamples-engine.position()<AheadThreshold &&
       !engine.isFading() && !engine.isFinished() && !romFadeComplete) {
        extending=true;extendEpoch=streamEpoch;
        auto session=romStream;
        extendJob=std::async(std::launch::async,[session, frames=RefillFrames]{return session->advanceFrames(frames);});
    }
#endif
    const uint64_t seek=seekRequested.exchange(UINT64_MAX);
    if(seek!=UINT64_MAX && loaded && !loadedVgm.empty() && playDuration.load() && !activeRomStream){
      seekBusy=true;
      // Remove queued sound so audio does not briefly play the old position.
      waveOutReset(wo);
      for(auto& b:buffers)b.header.dwFlags|=WHDR_DONE;
      const bool wasPaused=engine.isPaused();
      const uint64_t target=std::min(seek,playDuration.load()-1);
      // Locate the most recent valid chip-state snapshot. Forward seeks into
      // unplayed territory are exact but may need some time to synthesize.
      auto it=checkpoints.upper_bound(target);
      if(it!=checkpoints.begin())--it;
      if(it!=checkpoints.end())engine=it->second;
      else {std::string err;engine.load(loadedVgm,err);}
      std::array<int16_t,4096> discard{};
      while(engine.position()<target && !engine.isFinished() && running){
        const int step=(int)std::min<uint64_t>(2048,target-engine.position());
        engine.render(discard.data(),step);
        if(engine.position()/MegaDriveEngine::Rate/5>lastCheckpoint/5){
          const auto second=engine.position()/MegaDriveEngine::Rate;
          checkpoints[engine.position()]=engine;lastCheckpoint=second;
        }
      }
      if(wasPaused)engine.requestPause();
      playPosition=engine.position();seekBusy=false;
      PostMessageW(windowHandle,WM_STATUS,7,0);
    }
    bool didWork=false;
    for(auto& b:buffers){
      if(!(b.header.dwFlags & WHDR_DONE))continue;
      if(loaded && !engine.isFinished()){
        engine.setVolumes(volumePercent[0].load(),volumePercent[1].load(),
                          volumePercent[2].load(),volumePercent[3].load());
#ifdef MDA_ROM_AUDIO_ENABLED
        if(activeRomStream){
          // Never advance YM/PSG state beyond the Z80 trace horizon. If a
          // frame is late, pad with silence rather than permanently drifting
          // the audio clock ahead of the original driver's event timeline.
          const uint64_t available=streamTotalSamples>engine.position()
              ?streamTotalSamples-engine.position():0;
          const int n=static_cast<int>(std::min<uint64_t>(kFrames,available));
          if(n>0)engine.render(b.data,n);
          if(n<kFrames)std::memset(b.data+n*2,0,(kFrames-n)*2*sizeof(int16_t));
        }else
#endif
        engine.render(b.data,kFrames);
        playPosition=engine.position();
        const uint64_t sec=engine.position()/MegaDriveEngine::Rate;
        if(sec>=lastCheckpoint+5){
          checkpoints.emplace(engine.position(),engine);lastCheckpoint=sec;
        }
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
        12,105,555,213,hwnd,(HMENU)CTRL_LIST,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L"Open VGM / VGZ",WS_CHILD|WS_VISIBLE,415,10,152,31,hwnd,(HMENU)CTRL_OPEN,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L"Open ROM",WS_CHILD|WS_VISIBLE,302,10,110,31,hwnd,(HMENU)CTRL_OPEN_ROM,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L"<<",WS_CHILD|WS_VISIBLE,70,331,90,36,hwnd,(HMENU)CTRL_PREV,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L"Play / Pause",WS_CHILD|WS_VISIBLE,171,331,135,36,hwnd,(HMENU)CTRL_PLAY,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L"Stop",WS_CHILD|WS_VISIBLE,317,331,85,36,hwnd,(HMENU)CTRL_STOP,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L">>",WS_CHILD|WS_VISIBLE,413,331,90,36,hwnd,(HMENU)CTRL_NEXT,nullptr,nullptr);
      statusHandle=CreateWindowW(L"STATIC",L"Select PAL or NTSC, then open a VGM/VGZ",WS_CHILD|WS_VISIBLE,
        12,83,555,20,hwnd,nullptr,nullptr,nullptr);
      // Four independent mix buses; 100% is the original v0.4 mix level.
      const wchar_t* names[]={L"PSG",L"FM",L"DAC",L"Master"};
      const int ids[]={CTRL_PSG,CTRL_FM,CTRL_DAC,CTRL_MASTER};
      for(int i=0;i<4;i++){
        const int y=383+i*43;
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
                    WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,14,562,460,25,hwnd,
                    (HMENU)CTRL_COMBINED,nullptr,nullptr);
      CreateWindowW(L"BUTTON",L"Optional low-pass filter",
                    WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,14,594,210,25,hwnd,
                    (HMENU)CTRL_LOWPASS,nullptr,nullptr);
      lowpassSlider=CreateWindowExW(0,TRACKBAR_CLASSW,L"",WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS,
                    227,592,250,30,hwnd,(HMENU)CTRL_FILTER_STRENGTH,nullptr,nullptr);
      SendMessageW(lowpassSlider,TBM_SETRANGE,TRUE,MAKELPARAM(5,95));
      SendMessageW(lowpassSlider,TBM_SETPOS,TRUE,45);
      lowpassLabel=CreateWindowW(L"STATIC",L"45 %",WS_CHILD|WS_VISIBLE,484,595,75,24,hwnd,nullptr,nullptr,nullptr);
      HFONT font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
      for(HWND h: {listHandle,statusHandle,GetDlgItem(hwnd,CTRL_OPEN),GetDlgItem(hwnd,CTRL_OPEN_ROM),GetDlgItem(hwnd,CTRL_PREV),GetDlgItem(hwnd,CTRL_PLAY),GetDlgItem(hwnd,CTRL_STOP),GetDlgItem(hwnd,CTRL_NEXT)})
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
      if(id==CTRL_OPEN_ROM)openRom();
      if(id==CTRL_PROFILE && HIWORD(w)==CBN_SELCHANGE){
        const int profile=(int)SendMessageW(profileHandle,CB_GETCURSEL,0,0);
        if(profile>=0 && profile<=2)selectedTimingProfile=profile;
        PostMessageW(windowHandle,WM_STATUS,1,0);
      }
      if(id==CTRL_PAL || id==CTRL_NTSC){
        selectedRegion=(id==CTRL_PAL ? 50 : 60);
        PostMessageW(windowHandle,WM_STATUS,1,0);
      }
      if(id==CTRL_PLAY){
        if(romListMode){
#ifdef MDA_ROM_AUDIO_ENABLED
          int i=(int)SendMessageW(listHandle,LB_GETCURSEL,0,0);
          if(i>=0 && current.load()!=i) requestSong(i);
          else if(i>=0 && current.load()==i) pauseRequested=true;
#else
          showRomPending();
#endif
        }else pauseRequested=true;
      }
      if(id==CTRL_STOP)stopRequested=true;
      if(id==CTRL_PREV){if(romListMode){int i=(int)SendMessageW(listHandle,LB_GETCURSEL,0,0);int n=(int)romTracks.size();if(n>0){i=(i-1+n)%n;SendMessageW(listHandle,LB_SETCURSEL,i,0);requestSong(i);}}else prevRequested=true;}
      if(id==CTRL_NEXT){if(romListMode){int i=(int)SendMessageW(listHandle,LB_GETCURSEL,0,0);int n=(int)romTracks.size();if(n>0){i=(i+1)%n;SendMessageW(listHandle,LB_SETCURSEL,i,0);requestSong(i);}}else nextRequested=true;}
      if(id==CTRL_LIST && HIWORD(w)==LBN_DBLCLK){
#ifdef MDA_ROM_AUDIO_ENABLED
        requestSong((int)SendMessageW(listHandle,LB_GETCURSEL,0,0));
#else
        if(romListMode)showRomPending();else requestSong((int)SendMessageW(listHandle,LB_GETCURSEL,0,0));
#endif
      }
      return 0;
    }
    case WM_STATUS:{
      if(romListMode){
        if(w==6)showStatus(L"ROM: Starting persistent Z80 audio session...");
        else if(w==5)showStatus(L"ROM: Z80 driver could not generate sound for this selection");
        else if(w==3)showStatus(L"ROM: Failed to initialize audio engine");
        else if(w==4 || w==1){
          int i=current.load();
          if(i>=0 && i<(int)romTracks.size()){
            showStatus(L"ROM playback: "+widenAscii(romTracks[i].label)+L" | "+(selectedRegion.load()==50?L"PAL":L"NTSC")+L" | continuous original Z80 driver");
            SendMessageW(listHandle,LB_SETCURSEL,i,0);
          }
        }
        return 0;
      }
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
    CW_USEDEFAULT,CW_USEDEFAULT,595,690,nullptr,nullptr,inst,nullptr);
  if(!windowHandle)return 2;
  ShowWindow(windowHandle,show);UpdateWindow(windowHandle);
  audioThread=std::thread(audioWorker);
  MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
  running=false;if(audioThread.joinable())audioThread.join();return 0;
}
