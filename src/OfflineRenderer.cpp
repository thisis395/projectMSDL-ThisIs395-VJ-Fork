#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <projectM-4/projectM.h>
#include <projectM-4/parameters.h>
#include <projectM-4/audio.h>
#include <projectM-4/user_sprites.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
struct Clip {double start, duration, fade; std::string path;};
static Clip parseClip(const std::string& v) {
 auto a=v.find('|'),b=a==v.npos?a:v.find('|',a+1),c=b==v.npos?b:v.find('|',b+1);
 if(c==v.npos) throw std::runtime_error("Bad clip descriptor");
 return {std::stod(v.substr(0,a)),std::stod(v.substr(a+1,b-a-1)),std::stod(v.substr(b+1,c-b-1)),v.substr(c+1)};
}
static std::string stem(const std::string& path) {
 auto slash=path.find_last_of("/\\"); auto name=path.substr(slash==path.npos?0:slash+1);
 auto dot=name.find_last_of('.'); return name.substr(0,dot);
}
static std::string parentDir(const std::string& path) {
 auto slash=path.find_last_of("/\\"); return slash==path.npos?".":path.substr(0,slash);
}

int main(int argc, char** argv) {
 SDL_Window* win=nullptr; SDL_GLContext ctx=nullptr; projectm_handle pm=nullptr; bool sdl=false;
 try {
  int w=1920,h=1080,fps=60; double length=0, audioStart=0, audioGain=1;
  std::string pcmPath; std::vector<Clip> clips, sprites;
  for(int i=1;i<argc;++i) {
   if(i+1==argc) throw std::runtime_error("Missing option value");
   std::string k=argv[i],v=argv[++i];
   if(k=="--width") w=std::stoi(v);
   else if(k=="--height") h=std::stoi(v);
   else if(k=="--fps") fps=std::stoi(v);
   else if(k=="--duration") length=std::stod(v);
   else if(k=="--audio-start") audioStart=std::stod(v);
   else if(k=="--audio-gain") audioGain=std::stod(v);
   else if(k=="--pcm") pcmPath=v;
   else if(k=="--sprite") sprites.push_back(parseClip(v));
   else if(k=="--preset") clips.push_back(parseClip(v));
 else throw std::runtime_error("Unknown option");
  }
  if(w<16||w>7680||h<16||h>4320||fps<1||fps>240||length<=0||length>36000||clips.empty()||audioStart<0||!std::isfinite(audioStart)||!std::isfinite(audioGain)||audioGain<0||audioGain>2)
   throw std::runtime_error("Invalid geometry or timeline");
  std::sort(clips.begin(),clips.end(),[](const Clip&a,const Clip&b){return a.start<b.start;});
  for(size_t i=0;i<clips.size();++i)
   if(clips[i].start<0||clips[i].duration<=0||clips[i].fade<0||clips[i].path.empty()||
      (i&&clips[i].start+1.e-6<clips[i-1].start+clips[i-1].duration))
    throw std::runtime_error("Invalid or overlapping clips");
  for(const auto& sprite:sprites)
   if(sprite.start<0||sprite.duration<=0||sprite.fade<0||!std::isfinite(sprite.start)||
      !std::isfinite(sprite.duration)||!std::isfinite(sprite.fade)||sprite.path.empty())
    throw std::runtime_error("Invalid sprite timeline entry");
  std::ifstream pcm;
  if(!pcmPath.empty()) {
   pcm.open(pcmPath, std::ios::binary);
   if(!pcm) throw std::runtime_error("Cannot read decoded audio PCM");
  }
  if(SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError()); sdl=true;
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
  win=SDL_CreateWindow("projectM offline",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,
                       w,h,SDL_WINDOW_HIDDEN|SDL_WINDOW_OPENGL);
  if(!win) throw std::runtime_error(SDL_GetError());
  ctx=SDL_GL_CreateContext(win);
  if(!ctx) throw std::runtime_error(SDL_GetError());
  SDL_GL_MakeCurrent(win,ctx);
  int dw=0,dh=0; SDL_GL_GetDrawableSize(win,&dw,&dh);
  if(dw!=w||dh!=h) throw std::runtime_error("HiDPI resolution mismatch");
  pm=projectm_create(); if(!pm) throw std::runtime_error("projectM initialization failed");
  projectm_set_window_size(pm,w,h); projectm_set_fps(pm,fps);
  projectm_set_preset_duration(pm,36000.0); projectm_set_hard_cut_enabled(pm,false);
#ifdef _WIN32
  if(_setmode(_fileno(stdout),_O_BINARY)<0) throw std::runtime_error("Binary pipe mode failed");
#endif
  projectm_set_preset_locked(pm,true);
  std::set<std::string> spriteNames;
  std::vector<std::string> dirs;
  for(const auto& sprite:sprites) {
   if(!spriteNames.insert(stem(sprite.path)).second)
    throw std::runtime_error("Sprite PNG basenames must be unique");
   const std::string dir=parentDir(sprite.path);
   if(std::find(dirs.begin(),dirs.end(),dir)==dirs.end()) dirs.push_back(dir);
  }
  if(!dirs.empty()) {
   std::vector<const char*> paths;
   for(const auto& dir:dirs) paths.push_back(dir.c_str());
   projectm_set_texture_search_paths(pm, paths.data(),paths.size());
  }
  std::vector<uint32_t> spriteIds(sprites.size(),0);
  std::vector<float> pcmBuffer;
  // The source file is 48 kHz, stereo, little-endian float32.
  const unsigned int blockSamples=std::min(1024u,projectm_pcm_get_max_samples());
  std::vector<unsigned char> rgb(static_cast<size_t>(w)*h*3);
  glPixelStorei(GL_PACK_ALIGNMENT,1);
  size_t current=clips.size();
  const auto frames=static_cast<unsigned long long>(std::ceil(length*fps-1.e-9));
  for(unsigned long long frame=0;frame<frames;++frame) {
   const double t=static_cast<double>(frame)/fps; size_t next=0;
   for(size_t i=1;i<clips.size()&&t+1.e-6>=clips[i].start;++i) next=i;
   if(current!=next) {
    projectm_set_soft_cut_duration(pm,clips[next].fade);
    projectm_load_preset_file(pm,clips[next].path.c_str(),
                             current!=clips.size()&&clips[next].fade>0);
    current=next;
   }
   // FFT receives a time-indexed source audio window before the video frame.
   if(pcm && t>=audioStart && blockSamples>0) {
    const auto sampleIndex=static_cast<unsigned long long>((t-audioStart)*48000.0);
    const auto firstSample=sampleIndex>blockSamples?sampleIndex-blockSamples:0;
    pcmBuffer.assign(static_cast<size_t>(blockSamples)*2,0.0f);
    if(firstSample<static_cast<unsigned long long>(std::numeric_limits<std::streamoff>::max()/8)) {
     pcm.clear();
     pcm.seekg(static_cast<std::streamoff>(firstSample*8));
     if(pcm) pcm.read(reinterpret_cast<char*>(pcmBuffer.data()),
                       static_cast<std::streamsize>(pcmBuffer.size()*sizeof(float)));
     // A short read stays zero-padded for the final audio window.
     const std::streamsize got=pcm.gcount();
     if(got>0 && static_cast<size_t>(got)<pcmBuffer.size()*sizeof(float))
      std::fill(reinterpret_cast<unsigned char*>(pcmBuffer.data())+got,
                reinterpret_cast<unsigned char*>(pcmBuffer.data())+pcmBuffer.size()*sizeof(float),0);
     for(auto& sample:pcmBuffer) sample*=static_cast<float>(audioGain);
     projectm_pcm_add_float(pm,pcmBuffer.data(),blockSamples,PROJECTM_STEREO);
    }
   }
   // Sprite timelines use projectM user sprites and inherit its FFT bands.
   for(size_t i=0;i<sprites.size();++i) {
    const auto& s=sprites[i];
    const bool visible=t+1e-6>=s.start && t<s.start+s.duration-1e-6;
    if(visible && spriteIds[i]==0) {
     const std::string definition="img="+stem(s.path)+"\\ninit_1=burn=0;blendmode=4;\\n"
       "code_1=x=0.5;y=0.5;rot=0;a=vjopacity;\\n"
       "code_2=sx=0.25*(1+0.35*max(0,bass_att-1));sy=sx;\\n";
     spriteIds[i]=projectm_sprite_create(pm,"milkdrop",definition.c_str());
     if(!spriteIds[i]) throw std::runtime_error("Sprite creation failed: "+s.path);
    }
    if(!visible && spriteIds[i]) {
     projectm_sprite_destroy(pm,spriteIds[i]); spriteIds[i]=0;
    }
    if(visible && spriteIds[i]) {
     const double fade=std::min(s.fade,s.duration);
     const double elapsed=t-s.start, remaining=s.start+s.duration-t;
     const double opacity=fade>0?std::min(1.0,std::min(elapsed,remaining)/fade):1.0;
     projectm_sprite_set_var(pm,spriteIds[i],"vjopacity",std::max(0.0,opacity));
    }
   }
   projectm_set_frame_time(pm,t);
   glViewport(0,0,w,h); projectm_opengl_render_frame(pm);
   glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
   if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("OpenGL readback failed");
   if(std::fwrite(rgb.data(),1,rgb.size(),stdout)!=rgb.size())
    throw std::runtime_error("FFmpeg pipe write failed");
  }
  if(std::fflush(stdout)) throw std::runtime_error("Pipe flush failed");
  projectm_destroy(pm); pm=nullptr; SDL_GL_DeleteContext(ctx); ctx=nullptr;
  SDL_DestroyWindow(win); win=nullptr; SDL_QuitSubSystem(SDL_INIT_VIDEO); return 0;
 } catch(const std::exception& e) {
  std::fprintf(stderr,"Offline renderer: %s\n",e.what());
  if(pm) projectm_destroy(pm); if(ctx) SDL_GL_DeleteContext(ctx);
  if(win) SDL_DestroyWindow(win); if(sdl) SDL_QuitSubSystem(SDL_INIT_VIDEO); return 1;
 }
}
