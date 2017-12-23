#include "ImguiLibretro.h"
#include "Memory.h"
#include "CoreInfo.h"

#include "imguiext/imguial_button.h"
#include "imguiext/imguial_fonts.h"
#include "imguiext/imguifilesystem.h"
#include "imguiext/imguidock.h"

#include "json.hpp"

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <SDL.h>
#include <SDL_opengl.h>

// Ugh
using json = nlohmann::json;

static const char* s_gameControllerDB[] =
{
  // Updated on 2017-06-15
  #include "GameControllerDB.inl"
  // Some controllers I have around
  "63252305000000000000504944564944,USB Vibration Joystick (BM),platform:Windows,x:b3,a:b2,b:b1,y:b0,back:b8,start:b9,dpleft:h0.8,dpdown:h0.4,dpright:h0.2,dpup:h0.1,leftshoulder:b4,lefttrigger:b6,rightshoulder:b5,righttrigger:b7,leftstick:b10,rightstick:b11,leftx:a0,lefty:a1,rightx:a2,righty:a3,",
  "351212ab000000000000504944564944,NES30 Joystick,platform:Windows,x:b3,a:b2,b:b1,y:b0,back:b6,start:b7,leftshoulder:b4,rightshoulder:b5,leftx:a0,lefty:a1,"
};

class Application
{
protected:
  enum class State
  {
    kGetCorePath,
    kGetGamePath,
    kPaused,
    kRunning,
  };

  SDL_Window*       _window;
  SDL_GLContext     _glContext;
  SDL_AudioSpec     _audioSpec;
  SDL_AudioDeviceID _audioDev;

  Fifo   _fifo;
  Logger _logger;
  Config _config;
  Video  _video;
  Audio  _audio;
  Input  _input;
  Loader _loader;
  Memory _memory;

  Allocator<256 * 1024> _allocator;

  libretro::Components _components;

  State          _state;
  libretro::Core _core;
  std::string    _coreKey;
  std::string    _extensions;

  json        _appCfg;
  json        _coreCfg;
  json        _inputCfg;
  std::string _corePath;
  std::string _gamePath;

  static void s_audioCallback(void* udata, Uint8* stream, int len)
  {
    Application* app = (Application*)udata;
    size_t avail = app->_fifo.occupied();

    if (avail < (size_t)len)
    {
      app->_fifo.read((void*)stream, avail);
      memset((void*)(stream + avail), 0, len - avail);
    }
    else
    {
      app->_fifo.read((void*)stream, len);
    }
  }

public:
  Application()
  {
    _components._logger    = &_logger;
    _components._config    = &_config;
    _components._video     = &_video;
    _components._audio     = &_audio;
    _components._input     = &_input;
    _components._allocator = &_allocator;
    _components._loader    = &_loader;
  }

  bool init(const char* title, int width, int height)
  {
    if (!_logger.init())
    {
      return false;
    }

    {
      // Setup SDL
      if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
      {
        return false;
      }

      // Setup window
      _window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

      if (_window == NULL)
      {
        return false;
      }

      _glContext = SDL_GL_CreateContext(_window);

      if (_glContext == NULL)
      {
        SDL_DestroyWindow(_window);
        return false;
      }

      SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
      SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
      SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
      SDL_GL_SetSwapInterval(0);

      // Init audio
      SDL_AudioSpec want;
      memset(&want, 0, sizeof(want));

      want.freq = 44100;
      want.format = AUDIO_S16SYS;
      want.channels = 2;
      want.samples = 1024;
      want.callback = s_audioCallback;
      want.userdata = this;

      _audioDev = SDL_OpenAudioDevice(NULL, 0, &want, &_audioSpec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);

      if (_audioDev == 0)
      {
        SDL_GL_DeleteContext(_glContext);
        SDL_DestroyWindow(_window);
        return false;
      }

      if (!_fifo.init(&_logger, _audioSpec.size * 2))
      {
        SDL_CloseAudioDevice(_audioDev);
        SDL_GL_DeleteContext(_glContext);
        SDL_DestroyWindow(_window);
        return false;
      }

      SDL_PauseAudioDevice(_audioDev, 0);

      // Add controller mappings
      for (size_t i = 0; i < sizeof(s_gameControllerDB) / sizeof(s_gameControllerDB[0]); i++ )
      {
        SDL_GameControllerAddMapping(s_gameControllerDB[i]);
      }

      // Setup ImGui binding
      ImGui_ImplSdl_Init(_window);
    }

    {
      // Set Proggy Tiny as the default font
      int ttf_size;
      const void* ttf_data = ImGuiAl::Fonts::GetCompressedData(ImGuiAl::Fonts::kProggyTiny, &ttf_size);

      ImGuiIO& io = ImGui::GetIO();
      ImFont* font = io.Fonts->AddFontFromMemoryCompressedTTF(ttf_data, ttf_size, 10.0f);
      font->DisplayOffset.y = 1.0f;

      // Add icons from Font Awesome
      ImFontConfig config;
      config.MergeMode = true;
      config.PixelSnapH = true;

      static const ImWchar ranges1[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
      ttf_data = ImGuiAl::Fonts::GetCompressedData(ImGuiAl::Fonts::kFontAwesome, &ttf_size);
      io.Fonts->AddFontFromMemoryCompressedTTF(ttf_data, ttf_size, 12.0f, &config, ranges1);

      // Add Icons from Material Design
      static const ImWchar ranges2[] = {ICON_MIN_MD, ICON_MAX_MD, 0};
      ttf_data = ImGuiAl::Fonts::GetCompressedData(ImGuiAl::Fonts::kMaterialDesign, &ttf_size);
      io.Fonts->AddFontFromMemoryCompressedTTF(ttf_data, ttf_size, 24.0f, &config, ranges2);
    }

    {
      // Read core options and input configuration
      if (!_loader.init(&_logger))
      {
        SDL_CloseAudioDevice(_audioDev);
        _fifo.destroy();
        SDL_GL_DeleteContext(_glContext);
        SDL_DestroyWindow(_window);
        return false;
      }

      size_t size;
      void* data = _loader.load(&size, "config.json");

      if (data != NULL)
      {
        bool ok = true;

        // Fuck exceptions
        try
        {
          _appCfg = json::parse((char*)data, (char*)data + size);
        }
        catch (...)
        {
          ok = false;
        }

        _loader.free(data);

        if (!ok)
        {
          _appCfg = json::object();
        }
      }
      else
      {
        _appCfg = json::object();
      }

      if (_appCfg.find("corepath") == _appCfg.end())
      {
        _corePath = "";
      }
      else
      {
        _corePath = _appCfg["corepath"].get<std::string>();
      }

      if (_appCfg.find("gamepath") == _appCfg.end())
      {
        _gamePath = "";
      }
      else
      {
        _gamePath = _appCfg["gamepath"].get<std::string>();
      }

      if (_appCfg.find("corecfg") == _appCfg.end())
      {
        _appCfg["corecfg"] = json::object();
      }

      if (_appCfg.find("inputcfg") == _appCfg.end())
      {
        _appCfg["inputcfg"] = json::object();
      }

      _coreCfg = json::object();
      _inputCfg = json::object();
    }

    {
      // Initialize components
      bool ok = _config.init(&_logger, &_coreCfg);
      ok = ok && _video.init(&_logger);
      ok = ok && _audio.init(&_logger, _audioSpec.freq, &_fifo);
      ok = ok && _input.init(&_logger, &_inputCfg);
      ok = ok && _allocator.init(&_logger);
      ok = ok && _memory.init(&_core);

      if (!ok)
      {
        SDL_CloseAudioDevice(_audioDev);
        _fifo.destroy();
        SDL_GL_DeleteContext(_glContext);
        SDL_DestroyWindow(_window);
        return false;
      }
    }

    {
      // Add controllers already connected
      int max = SDL_NumJoysticks();

      for(int i = 0; i < max; i++)
      {
        _input.addController(i);
      }
    }

    _state = State::kGetCorePath;
    ImGui::LoadDock("imguidock.ini");
    return true;
  }

  void destroy()
  {
    ImGui::SaveDock("imguidock.ini");

    {
      _appCfg["corecfg"][_coreKey] = _coreCfg;
      _appCfg["inputcfg"][_coreKey] = _inputCfg;
      _appCfg["corepath"] = _corePath;
      _appCfg["gamepath"] = _gamePath;

      std::string cfg = _appCfg.dump(2);
      FILE* file = fopen("config.json", "wb");
      fwrite(cfg.c_str(), 1, cfg.size(), file);
      fclose(file);
    }

    _memory.destroy();
    _input.destroy();
    _audio.destroy();
    _video.destroy();
    _config.destroy();
    _logger.destroy();

    ImGui_ImplSdl_Shutdown();
    SDL_CloseAudioDevice(_audioDev);
    _fifo.destroy();
    SDL_GL_DeleteContext(_glContext);
    SDL_DestroyWindow(_window);
    SDL_Quit();
  }

  void run()
  {
    bool done = false;

    do
    {
      SDL_Event event;

      while (SDL_PollEvent(&event))
      {
        if (!ImGui_ImplSdl_ProcessEvent(&event))
        {
          switch (event.type)
          {
          case SDL_QUIT:
            done = true;
            break;

          case SDL_CONTROLLERDEVICEADDED:
          case SDL_CONTROLLERDEVICEREMOVED:
          case SDL_CONTROLLERBUTTONUP:
          case SDL_CONTROLLERBUTTONDOWN:
          case SDL_CONTROLLERAXISMOTION:
            _input.processEvent(&event);
            break;
          }
        }
      }

      if (_state == State::kRunning)
      {
        _core.step();
      }

      ImGui_ImplSdl_NewFrame(_window);
      draw();
      glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);

      glClearColor(0.05f, 0.05f, 0.05f, 0);
      glClear(GL_COLOR_BUFFER_BIT);

      ImGui::Render();
      SDL_GL_SwapWindow(_window);
      SDL_Delay(1);
    }
    while (!done);
  }

  void drawCoreControls()
  {
    ImVec2 size = ImVec2(100.0f, 0.0f);

    bool loadCorePressed = ImGuiAl::Button(ICON_FA_COG " Load core...", _state == State::kGetCorePath, size);
    ImGui::SameLine();

    bool loadGamePressed = ImGuiAl::Button(ICON_FA_ROCKET " Load game...", _state == State::kGetGamePath, size);
    ImGui::SameLine();

    bool pressed = ImGuiAl::Button(ICON_FA_PLAY " Run", _state == State::kPaused, size);
    ImGui::SameLine();

    if (pressed)
    {
      _logger.printf(RETRO_LOG_DEBUG, "Running");
      _state = State::kRunning;
    }

    pressed = ImGuiAl::Button(ICON_FA_PAUSE " Pause", _state == State::kRunning, size);
    ImGui::SameLine();

    if (pressed)
    {
      _logger.printf(RETRO_LOG_DEBUG, "Paused");
      _state = State::kPaused;
    }

    pressed = ImGuiAl::Button(ICON_FA_STOP " Stop", _state == State::kPaused, size);

    if (pressed)
    {
      _logger.printf(RETRO_LOG_DEBUG, "Stopped");

      _fifo.reset();
      _config.reset();
      _video.reset();
      _audio.reset();
      _input.reset();
      _loader.reset();
      _allocator.reset();
      _coreCfg = json::object();
      _inputCfg = json::object();
      _extensions.clear();
      _core.destroy();
      _memory.reset();

      _state = State::kGetCorePath;
    }

    if (_state == State::kPaused || _state == State::kRunning)
    {
      drawCoreInfo(&_core);
    }

    // Show the file open dialogs
    {
      static ImGuiFs::Dialog coreDialog;

      ImVec2 fileDialogPos = ImVec2(
        (ImGui::GetIO().DisplaySize.x - coreDialog.WindowSize.x) / 2.0f,
        (ImGui::GetIO().DisplaySize.y - coreDialog.WindowSize.y) / 2.0f
      );

#ifdef _WIN32
      const char* path = coreDialog.chooseFileDialog(loadCorePressed, _corePath.c_str(), ".dll", "Open Core", coreDialog.WindowSize, fileDialogPos);
#else
      const char* path = coreDialog.chooseFileDialog(loadCorePressed, _corePath.c_str(), ".so", "Open Core", coreDialog.WindowSize, fileDialogPos);
#endif

      if (strlen(path) > 0)
      {
        // Get the core configuration before initializing
        char temp[ImGuiFs::MAX_PATH_BYTES];
        char temp2[ImGuiFs::MAX_PATH_BYTES];
        ImGuiFs::PathGetFileName(path, temp);
        ImGuiFs::PathGetFileNameWithoutExtension(temp, temp2);
        _coreKey = temp2;

        json cfg = _appCfg["corecfg"];

        if (cfg.find(_coreKey) != cfg.end())
        {
          _coreCfg = cfg[_coreKey];
        }

        cfg = _appCfg["inputcfg"];

        if (cfg.find(_coreKey) != cfg.end())
        {
          _inputCfg = cfg[_coreKey];
        }
        
        _core.init(&_components);

        if (_core.loadCore(path))
        {
          ImGuiFs::PathGetDirectoryName(path, temp);
          _corePath = temp;

          const char* ext = _core.getSystemInfo()->valid_extensions;

          if (ext != NULL)
          {
            for (;;)
            {
              const char* begin = ext;

              while (*ext != 0 && *ext != '|')
              {
                ext++;
              }

              _extensions.append(".");
              _extensions.append(begin, ext - begin);

              if (*ext == 0)
              {
                break;
              }

              _extensions.append( ";" );
              ext++;
            }
          }

          _state = State::kGetGamePath;
        }
      }

      static ImGuiFs::Dialog gameDialog;
      path = gameDialog.chooseFileDialog(loadGamePressed, _gamePath.c_str(), _extensions.c_str(), "Open Game", coreDialog.WindowSize, fileDialogPos);

      if (strlen(path) > 0)
      {
        if (_core.loadGame(path))
        {
          char temp[ImGuiFs::MAX_PATH_BYTES];
          ImGuiFs::PathGetDirectoryName(path, temp);
          _gamePath = temp;
          
          _state = State::kRunning;
        }
        else
        {
          _fifo.reset();
          _config.reset();
          _video.reset();
          _audio.reset();
          _input.reset();
          _loader.reset();
          _allocator.reset();
          _coreCfg = json::object();
          _inputCfg = json::object();
          _extensions.clear();

          _state = State::kGetCorePath;
        }
      }
    }
  }

  void draw()
  {
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;

    const float oldWindowRounding = ImGui::GetStyle().WindowRounding;
    ImGui::GetStyle().WindowRounding = 0;
    const bool visible = ImGui::Begin("Docking Manager", NULL, ImVec2(0, 0), 1.0f, flags);
    ImGui::GetStyle().WindowRounding = oldWindowRounding;

    if (visible)
    {
      ImGui::BeginDockspace();

      if (ImGui::BeginDock(ICON_FA_COG " Core"))
        drawCoreControls();
      ImGui::EndDock();

      if (ImGui::BeginDock(ICON_FA_COMMENT " Log"))
        _logger.draw();
      ImGui::EndDock();
      
      if (ImGui::BeginDock(ICON_FA_WRENCH " Configuration"))
        _config.draw();
      ImGui::EndDock();
      
      if (ImGui::BeginDock(ICON_FA_DESKTOP " Video"))
        _video.draw();
      ImGui::EndDock();
      
      if (ImGui::BeginDock(ICON_FA_VOLUME_UP " Audio"))
        _audio.draw();
      ImGui::EndDock();
      
      if (ImGui::BeginDock(ICON_FA_GAMEPAD " Input"))
        _input.draw();
      ImGui::EndDock();

      _memory.draw(_state == State::kRunning);
      
      ImGui::EndDockspace();
    }

    ImGui::End();
  }
};

int main(int, char**)
{
  Application app;

  if (!app.init("Cheevos Hunter", 1024, 640))
  {
    return 1;
  }

  app.run();
  app.destroy();
  return 0;
}
