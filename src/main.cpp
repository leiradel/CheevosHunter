#include "ImguiLibretro.h"
#include "Memory.h"

#include "imgui/imgui_impl_sdl.h"
#include "imguiext/imguial_button.h"
#include "imguiext/imguial_fonts.h"
#include "imguiext/imguifilesystem.h"

#include <imgui.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

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
  std::string    _extensions;
  std::string    _coreFolder;
  std::string    _gameFolder;

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
      // Initialize components
      bool ok = _config.init(&_logger);
      ok = ok && _video.init(&_logger);
      ok = ok && _audio.init(&_logger, _audioSpec.freq, &_fifo);
      ok = ok && _input.init(&_logger);
      ok = ok && _loader.init(&_logger);
      ok = ok && _allocator.init(&_logger);

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
    return true;
  }

  void destroy()
  {
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

  static void table(int columns, ...)
  {
    ImGui::Columns(columns, NULL, true);

    va_list args;
    va_start(args, columns);

    for (;;)
    {
      const char* name = va_arg(args, const char*);

      if (name == NULL)
      {
        break;
      }

      ImGui::Separator();
      ImGui::Text("%s", name);
      ImGui::NextColumn();

      for (int col = 1; col < columns; col++)
      {
        switch (va_arg(args, int))
        {
        case 's': ImGui::Text("%s", va_arg(args, const char*)); break;
        case 'd': ImGui::Text("%d", va_arg(args, int)); break;
        case 'u': ImGui::Text("%u", va_arg(args, unsigned)); break;
        case 'f': ImGui::Text("%f", va_arg(args, double)); break;
        case 'b': ImGui::Text("%s", va_arg(args, int) ? "true" : "false"); break;
        }

        ImGui::NextColumn();
      }
    }

    va_end(args);

    ImGui::Columns(1);
    ImGui::Separator();
  }

  void draw()
  {
    bool loadCorePressed, loadGamePressed;

    // Draw the core lifecycle controls
    if (ImGui::Begin(ICON_FA_COG " Core"))
    {
      ImVec2 size = ImVec2(100.0f, 0.0f);

      loadCorePressed = ImGuiAl::Button(ICON_FA_COG " Load core...", _state == State::kGetCorePath, size);
      ImGui::SameLine();

      loadGamePressed = ImGuiAl::Button(ICON_FA_ROCKET " Load game...", _state == State::kGetGamePath, size);
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
        _extensions.clear();
        _memory.destroy();
        _core.destroy();

        _state = State::kGetCorePath;
      }
    }

    if (_state == State::kPaused || _state == State::kRunning)
    {
      if (ImGui::CollapsingHeader("Basic Information"))
      {
        static const char* pixel_formats[] =
        {
          "0RGB1555", "XRGB8888", "RGB565"
        };
        
        enum retro_pixel_format ndx = _core.getPixelFormat();
        const char* pixel_format = "Unknown";

        if (ndx != RETRO_PIXEL_FORMAT_UNKNOWN)
        {
          pixel_format = pixel_formats[ndx];
        }

        table(
          2,
          "Api Version",           'u', _core.getApiVersion(),
          "Region",                's', _core.getRegion() == RETRO_REGION_NTSC ? "NTSC" : "PAL",
          "Rotation",              'u', _core.getRotation() * 90,
          "Performance level",     'u', _core.getPerformanceLevel(),
          "Pixel Format",          's', pixel_format,
          "Supports no Game",      'b', _core.getSupportsNoGame(),
          "Supports Achievements", 'b', _core.getSupportAchievements(),
          "Save RAM Size",         'u', _core.getMemorySize(RETRO_MEMORY_SAVE_RAM),
          "RTC Size",              'u', _core.getMemorySize(RETRO_MEMORY_RTC),
          "System RAM Size",       'u', _core.getMemorySize(RETRO_MEMORY_SYSTEM_RAM),
          "Video RAM Size",        'u', _core.getMemorySize(RETRO_MEMORY_VIDEO_RAM),
          NULL
        );
      }

      if (ImGui::CollapsingHeader("retro_system_info"))
      {
        const struct retro_system_info* info = _core.getSystemInfo();

        table(
          2,
          "library_name",     's', info->library_name,
          "library_version",  's', info->library_version,
          "valid_extensions", 's', info->valid_extensions,
          "need_fullpath",    'b', info->need_fullpath,
          "block_extract",    'b', info->block_extract,
          NULL
        );
      }

      if (ImGui::CollapsingHeader("retro_system_av_info"))
      {
        const struct retro_system_av_info* av_info = _core.getSystemAVInfo();

        table(
          2,
          "base_width",   'u', av_info->geometry.base_width,
          "base_height",  'u', av_info->geometry.base_height,
          "max_width",    'u', av_info->geometry.max_width,
          "max_height",   'u', av_info->geometry.max_height,
          "aspect_ratio", 'f', av_info->geometry.aspect_ratio,
          "fps",          'f', av_info->timing.fps,
          "sample_rate",  'f', av_info->timing.sample_rate,
          NULL
        );
      }

      if (ImGui::CollapsingHeader("retro_input_descriptor"))
      {
        unsigned count;
        const struct retro_input_descriptor* desc = _core.getInputDescriptors(&count);
        const struct retro_input_descriptor* end = desc + count;

        ImVec2 min = ImGui::GetWindowContentRegionMin();
        ImVec2 max = ImGui::GetWindowContentRegionMax();
        max.x -= min.x;

        max.y = ImGui::GetItemsLineHeightWithSpacing();

        ImGui::BeginChild("##empty", max);
        ImGui::Columns(5, NULL, true);
        ImGui::Separator();
        ImGui::Text("port");
        ImGui::NextColumn();
        ImGui::Text("device");
        ImGui::NextColumn();
        ImGui::Text("index");
        ImGui::NextColumn();
        ImGui::Text("id");
        ImGui::NextColumn();
        ImGui::Text("description");
        ImGui::NextColumn();
        ImGui::Columns( 1 );
        ImGui::Separator();
        ImGui::EndChild();

        max.y = ImGui::GetItemsLineHeightWithSpacing() * (count < 16 ? count : 16);

        ImGui::BeginChild("retro_input_descriptor", max);
        ImGui::Columns(5, NULL, true);

        for (; desc < end; desc++)
        {
          static const char* device_names[] =
          {
            "None", "Joypad", "Mouse", "Keyboard", "Lightgun", "Analog", "Pointer"
          };

          static const char* button_names[] =
          {
            "B", "Y", "Select", "Start", "Up", "Down", "Left",
            "Right", "A", "X", "L", "R", "L2", "R2", "L3", "R3"
          };

          ImGui::Separator();
          ImGui::Text("%u", desc->port);
          ImGui::NextColumn();
          ImGui::Text("(%u) %s", desc->device, device_names[desc->device]);
          ImGui::NextColumn();
          ImGui::Text("%u", desc->index);
          ImGui::NextColumn();
          ImGui::Text("(%2u) %s", desc->id, button_names[desc->id]);
          ImGui::NextColumn();
          ImGui::Text("%s", desc->description);
          ImGui::NextColumn();
        }

        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::EndChild();
      }

      if (ImGui::CollapsingHeader("retro_controller_info"))
      {
        unsigned count;
        const struct retro_controller_info* info = _core.getControllerInfo(&count);
        const struct retro_controller_info* end = info + count;

        ImGui::Columns(3, NULL, true);
        ImGui::Separator();
        ImGui::Text("port");
        ImGui::NextColumn();
        ImGui::Text("desc");
        ImGui::NextColumn();
        ImGui::Text("id");
        ImGui::NextColumn();

        for (count = 0; info < end; count++, info++)
        {
          const struct retro_controller_description* type = info->types;
          const struct retro_controller_description* end = type + info->num_types;

          for (; type < end; type++)
          {
            static const char* device_names[] =
            {
              "None", "Joypad", "Mouse", "Keyboard", "Lightgun", "Analog", "Pointer"
            };

            unsigned device = type->id & RETRO_DEVICE_MASK;

            ImGui::Separator();
            ImGui::Text("%u", count);
            ImGui::NextColumn();
            ImGui::Text("%s", type->desc);
            ImGui::NextColumn();
            ImGui::Text("(0x%04X) %s", type->id, device_names[device]);
            ImGui::NextColumn();
          }
        }

        ImGui::Columns(1);
        ImGui::Separator();
      }

      if (ImGui::CollapsingHeader("retro_variable"))
      {
        unsigned count;
        const struct retro_variable* var = _core.getVariables(&count);
        const struct retro_variable* end = var + count;

        ImGui::Columns(2, NULL, true);
        ImGui::Separator();
        ImGui::Text("key");
        ImGui::NextColumn();
        ImGui::Text("value");
        ImGui::NextColumn();

        for (; var < end; var++)
        {
          ImGui::Separator();
          ImGui::Text("%s", var->key);
          ImGui::NextColumn();
          ImGui::Text("%s", var->value);
          ImGui::NextColumn();
        }

        ImGui::Columns(1);
        ImGui::Separator();
      }

      if (ImGui::CollapsingHeader("retro_subsystem_info"))
      {
        unsigned count;
        const struct retro_subsystem_info* info = _core.getSubsystemInfo(&count);
        const struct retro_subsystem_info* end = info + count;

        for (; info < end; info++)
        {
          ImGui::Columns(3, NULL, true);
          ImGui::Separator();
          ImGui::Text("desc");
          ImGui::NextColumn();
          ImGui::Text("ident");
          ImGui::NextColumn();
          ImGui::Text("id");
          ImGui::NextColumn();

          ImGui::Separator();
          ImGui::Text("%s", info->desc);
          ImGui::NextColumn();
          ImGui::Text("%s", info->ident);
          ImGui::NextColumn();
          ImGui::Text("%u", info->id);
          ImGui::NextColumn();

          ImGui::Columns(1);
          ImGui::Separator();

          ImGui::Indent();

          const struct retro_subsystem_rom_info* rom = info->roms;
          const struct retro_subsystem_rom_info* end = rom + info->num_roms;

          for (unsigned i = 0; rom < end; i++, rom++)
          {
            char title[64];
            snprintf(title, sizeof(title), "retro_subsystem_rom_info[%u]", i);

            if (ImGui::CollapsingHeader(title))
            {
              ImGui::Columns(5, NULL, true);
              ImGui::Separator();
              ImGui::Text("desc" );
              ImGui::NextColumn();
              ImGui::Text("valid_extensions");
              ImGui::NextColumn();
              ImGui::Text("need_fullpath");
              ImGui::NextColumn();
              ImGui::Text("block_extract");
              ImGui::NextColumn();
              ImGui::Text("required");
              ImGui::NextColumn();

              ImGui::Separator();
              ImGui::Text("%s", rom->desc);
              ImGui::NextColumn();
              ImGui::Text("%s", rom->valid_extensions);
              ImGui::NextColumn();
              ImGui::Text("%s", rom->need_fullpath ? "true" : "false");
              ImGui::NextColumn();
              ImGui::Text("%s", rom->block_extract ? "true" : "false");
              ImGui::NextColumn();
              ImGui::Text("%s", rom->required ? "true" : "false");
              ImGui::NextColumn();

              ImGui::Columns(1);
              ImGui::Separator();

              ImGui::Indent();

              const struct retro_subsystem_memory_info* mem = rom->memory;
              const struct retro_subsystem_memory_info* end = mem + rom->num_memory;

              for (unsigned j = 0; mem < end; j++, mem++)
              {
                char title[64];
                snprintf(title, sizeof(title), "retro_subsystem_memory_info[%u]", j);

                ImGui::Columns(3, NULL, true);
                ImGui::Separator();
                ImGui::Text("");
                ImGui::NextColumn();
                ImGui::Text("extension");
                ImGui::NextColumn();
                ImGui::Text("type");
                ImGui::NextColumn();

                ImGui::Separator();
                ImGui::Text("%s", title);
                ImGui::NextColumn();
                ImGui::Text("%s", mem->extension);
                ImGui::NextColumn();
                ImGui::Text("0x%08X", mem->type);
                ImGui::NextColumn();
              }

              ImGui::Unindent();
            }

            ImGui::Unindent();
          }
        }
      }

      if (ImGui::CollapsingHeader("retro_memory_map"))
      {
        const struct retro_memory_map* mmap = _core.getMemoryMap();
        const struct retro_memory_descriptor* desc = mmap->descriptors;
        const struct retro_memory_descriptor* end = desc + mmap->num_descriptors;

        ImGui::Columns(8, NULL, true);
        ImGui::Separator();
        ImGui::Text("flags");
        ImGui::NextColumn();
        ImGui::Text("ptr");
        ImGui::NextColumn();
        ImGui::Text("offset");
        ImGui::NextColumn();
        ImGui::Text("start");
        ImGui::NextColumn();
        ImGui::Text("select");
        ImGui::NextColumn();
        ImGui::Text("disconnect");
        ImGui::NextColumn();
        ImGui::Text("len");
        ImGui::NextColumn();
        ImGui::Text("addrspace");
        ImGui::NextColumn();

        for (; desc < end; desc++)
        {
          char flags[7];

          flags[0] = 'M';

          if ((desc->flags & RETRO_MEMDESC_MINSIZE_8) == RETRO_MEMDESC_MINSIZE_8)
          {
            flags[1] = '8';
          }
          else if ((desc->flags & RETRO_MEMDESC_MINSIZE_4) == RETRO_MEMDESC_MINSIZE_4)
          {
            flags[1] = '4';
          }
          else if ((desc->flags & RETRO_MEMDESC_MINSIZE_2) == RETRO_MEMDESC_MINSIZE_2)
          {
            flags[1] = '2';
          }
          else
          {
            flags[1] = '1';
          }

          flags[2] = 'A';

          if ((desc->flags & RETRO_MEMDESC_ALIGN_8) == RETRO_MEMDESC_ALIGN_8)
          {
            flags[3] = '8';
          }
          else if ((desc->flags & RETRO_MEMDESC_ALIGN_4) == RETRO_MEMDESC_ALIGN_4)
          {
            flags[3] = '4';
          }
          else if ((desc->flags & RETRO_MEMDESC_ALIGN_2) == RETRO_MEMDESC_ALIGN_2)
          {
            flags[3] = '2';
          }
          else
          {
            flags[3] = '1';
          }

          flags[4] = desc->flags & RETRO_MEMDESC_BIGENDIAN ? 'B' : 'b';
          flags[5] = desc->flags & RETRO_MEMDESC_CONST ? 'C' : 'c';
          flags[6] = 0;

          ImGui::Separator();
          ImGui::Text("%s", flags);
          ImGui::NextColumn();
          ImGui::Text("%p", desc->ptr);
          ImGui::NextColumn();
          ImGui::Text("0x%08X", desc->offset);
          ImGui::NextColumn();
          ImGui::Text("0x%08X", desc->start);
          ImGui::NextColumn();
          ImGui::Text("0x%08X", desc->select);
          ImGui::NextColumn();
          ImGui::Text("0x%08X", desc->disconnect);
          ImGui::NextColumn();
          ImGui::Text("0x%08X", desc->len);
          ImGui::NextColumn();
          ImGui::Text("%s", desc->addrspace);
          ImGui::NextColumn();
        }

        ImGui::Columns(1);
        ImGui::Separator();
      }
    }

    ImGui::End();

    // Show the file open dialogs
    {
      static ImGuiFs::Dialog coreDialog;

      ImVec2 fileDialogPos = ImVec2(
        (ImGui::GetIO().DisplaySize.x - coreDialog.WindowSize.x) / 2.0f,
        (ImGui::GetIO().DisplaySize.y - coreDialog.WindowSize.y) / 2.0f
      );

#ifdef _WIN32
      const char* path = coreDialog.chooseFileDialog(loadCorePressed, _coreFolder.c_str(), ".dll", "Open Core", coreDialog.WindowSize, fileDialogPos);
#else
      const char* path = coreDialog.chooseFileDialog(loadCorePressed, _coreFolder.c_str(), ".so", "Open Core", coreDialog.WindowSize, fileDialogPos);
#endif

      if (strlen(path) > 0)
      {
        _core.init(&_components);

        if (_core.loadCore(path))
        {
          char folder[ImGuiFs::MAX_PATH_BYTES];
          ImGuiFs::PathGetDirectoryName(path, folder);
          _coreFolder = folder;
          
          _state = State::kGetGamePath;
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
        }
      }

      static ImGuiFs::Dialog gameDialog;
      path = gameDialog.chooseFileDialog(loadGamePressed, _gameFolder.c_str(), _extensions.c_str(), "Open Game", coreDialog.WindowSize, fileDialogPos);

      if (strlen(path) > 0)
      {
        if (_core.loadGame(path))
        {
          char folder[ImGuiFs::MAX_PATH_BYTES];
          ImGuiFs::PathGetDirectoryName(path, folder);
          _gameFolder = folder;
          
          _memory.init(&_core);
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
          _extensions.clear();

          _state = State::kGetCorePath;
        }
      }
    }

    _logger.draw();
    _config.draw();
    _video.draw();
    _audio.draw();
    _input.draw();

    if (_state == State::kPaused || _state == State::kRunning)
    {
      _memory.draw();
    }
  }
};

int main(int, char**)
{
  Application app;

  if (!app.init("Cheevos Explorer", 1024, 640))
  {
    return 1;
  }

  app.run();
  app.destroy();
  return 0;
}
