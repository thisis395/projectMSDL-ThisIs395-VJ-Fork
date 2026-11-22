#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <projectM-4/projectM.h>
#include <projectM-4/parameters.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
struct Clip {double start, duration, fade; std::string path;};
int main(int argc, char** argv) {
 SDL_Window* win=nullptr; SDL_GLContext ctx=nullptr; projectm_handle pm=nullptr; bool sdl=false;
 try {
  int w=1920,h=1080,fps=60; double length=0; std::vector<Clip> clips;
  for(int i=1;i<argc;++i) {
   if(i+1==argc) throw std::runtime_error("Missing option value");
   std::string k=argv[i],v=argv[++i];
   if(k=="--width") w=std::stoi(v);
   else if(k=="--height") h=std::stoi(v);
   else if(k=="--fps") fps=std::stoi(v);
   else if(k=="--duration") length=std::stod(v);
   else if(k=="--preset") {
    auto a=v.find('|'),b=a==v.npos?a:v.find('|',a+1),c=b==v.npos?b:v.find('|',b+1);
    if(c==v.npos) throw std::runtime_error("Bad preset descriptor");
    clips.push_back({std::stod(v.substr(0,a)),std::stod(v.substr(a+1,b-a-1)),
     std::stod(v.substr(b+1,c-b-1)),v.substr(c+1)});
   } else throw std::runtime_error("Unknown option");
  }
  if(w<16||w>7680||h<16||h>4320||fps<1||fps>240||length<=0||length>36000||clips.empty())
   throw std::runtime_error("Invalid geometry or timeline");
  std::sort(clips.begin(),clips.end(),[](const Clip&a,const Clip&b){return a.start<b.start;});
  for(size_t i=0;i<clips.size();++i)
   if(clips[i].start<0||clips[i].duration<=0||clips[i].fade<0||clips[i].path.empty()||
      (i&&clips[i].start+1.e-6<clips[i-1].start+clips[i-1].duration))
    throw std::runtime_error("Invalid or overlapping clips");
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
