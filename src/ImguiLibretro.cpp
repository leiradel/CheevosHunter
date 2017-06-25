#include "ImguiLibretro.h"

#include "fnkdat/fnkdat.h"
#include "imguiext/imguial_fonts.h"

#include <sys/stat.h>
#include <malloc.h>
#include <math.h>

static const uint32_t s_controller[] =
{
  #include "controller.inl"
};

bool Fifo::init(libretro::LoggerComponent* logger, size_t size)
{
  _mutex = SDL_CreateMutex();
  
  if (!_mutex)
  {
    return false;
  }
  
  _buffer = (uint8_t*)malloc(size);
  
  if (_buffer == NULL)
  {
    SDL_DestroyMutex(_mutex);
    return false;
  }
  
  _size = _avail = size;
  _first = _last = 0;
  _logger = logger;
  return true;
}

void Fifo::destroy()
{
  ::free(_buffer);
  SDL_DestroyMutex(_mutex);
}

void Fifo::reset()
{
  _avail = _size;
  _first = _last = 0;
}

void Fifo::read(void* data, size_t size)
{
  SDL_LockMutex(_mutex);

  size_t first = size;
  size_t second = 0;
  
  if (first > _size - _first)
  {
    first = _size - _first;
    second = size - first;
  }
  
  uint8_t* src = _buffer + _first;
  memcpy(data, src, first);
  memcpy((uint8_t*)data + first, _buffer, second);
  
  _first = (_first + size) % _size;
  _avail += size;

  SDL_UnlockMutex(_mutex);
}

void Fifo::write(const void* data, size_t size)
{
  SDL_LockMutex(_mutex);

  size_t first = size;
  size_t second = 0;
  
  if (first > _size - _last)
  {
    first = _size - _last;
    second = size - first;
  }
  
  uint8_t* dest = _buffer + _last;
  memcpy(dest, data, first);
  memcpy(_buffer, (uint8_t*)data + first, second);
  
  _last = (_last + size) % _size;
  _avail -= size;

  SDL_UnlockMutex(_mutex);
}

size_t Fifo::occupied()
{
  size_t avail;

  SDL_LockMutex(_mutex);
  avail = _size - _avail;
  SDL_UnlockMutex(_mutex);

  return avail;
}

size_t Fifo::free()
{
  size_t avail;

  SDL_LockMutex(_mutex);
  avail = _avail;
  SDL_UnlockMutex(_mutex);

  return avail;
}

bool Logger::init()
{
  static const char* actions[] =
  {
    ICON_FA_FILES_O " Copy",
    ICON_FA_FILE_O " Clear",
    NULL
  };

  if (!_logger.Init(0, actions))
  {
    return false;
  }

  _logger.SetLabel(ImGuiAl::Log::kDebug, ICON_FA_BUG " Debug");
  _logger.SetLabel(ImGuiAl::Log::kInfo, ICON_FA_INFO " Info");
  _logger.SetLabel(ImGuiAl::Log::kWarn, ICON_FA_EXCLAMATION_TRIANGLE " Warn");
  _logger.SetLabel(ImGuiAl::Log::kError, ICON_FA_BOMB " Error");
  _logger.SetCumulativeLabel(ICON_FA_SORT_AMOUNT_DESC " Cumulative");
  _logger.SetFilterHeaderLabel(ICON_FA_FILTER " Filters");
  _logger.SetFilterLabel(ICON_FA_SEARCH " Filter (inc,-exc)");

  static const char* buttons[] = {"Yes", "No", NULL};
  return _clear.Init("Clear Log?", ICON_MD_WARNING, "Do you want to clear the log history?", buttons, true);
}

void Logger::destroy()
{
}

void Logger::reset()
{
  _logger.Clear();
}

void Logger::draw()
{
  if (ImGui::Begin(ICON_FA_COMMENT " Log"))
  {
    switch (_logger.Draw())
    {
    case 1:
      {
        struct Iterator
        {
          static bool func(ImGuiAl::Log::Level level, const char* line, void* ud)
          {
            std::string* buffer = (std::string*)ud;

            switch (level)
            {
              case ImGuiAl::Log::kDebug: *buffer += "[DEBUG] "; break;
              case ImGuiAl::Log::kInfo:  *buffer += "[INFO ] "; break;
              case ImGuiAl::Log::kWarn:  *buffer += "[WARN ] "; break;
              case ImGuiAl::Log::kError: *buffer += "[ERROR] "; break;
            }

            *buffer += line;
            *buffer += '\n';

            return true;
          }
        };

        std::string buffer;
        _logger.Iterate(Iterator::func, true, &buffer);
        ImGui::SetClipboardText(buffer.c_str());
      }
      break;

    case 2:
      _clear.Open();
      break;
    }
  }

  ImGui::End();

  if (_clear.Draw() == 1)
  {
    _logger.Clear();
  }
}

void Logger::vprintf(enum retro_log_level level, const char* fmt, va_list args)
{
  ImGuiAl::Log::Level lvl;

  switch (level)
  {
  case RETRO_LOG_DEBUG: lvl = ImGuiAl::Log::kDebug; break;
  case RETRO_LOG_INFO:  lvl = ImGuiAl::Log::kInfo; break;
  case RETRO_LOG_WARN:  lvl = ImGuiAl::Log::kWarn; break;
  default: // fallthrough
  case RETRO_LOG_ERROR: lvl = ImGuiAl::Log::kError; break;
  }

  _logger.VPrintf(lvl, fmt, args);
}

bool Config::init(libretro::LoggerComponent* logger)
{
  _logger = logger;

  if (fnkdat(NULL, 0, 0, FNKDAT_INIT) != 0)
  {
    _logger->printf(RETRO_LOG_ERROR, "Error initializing fnkdat");
    return false;
  }

  char path[1024];

  if (fnkdat("system", path, sizeof(path), FNKDAT_VAR | FNKDAT_DATA | FNKDAT_CREAT) != 0)
  {
    _logger->printf(RETRO_LOG_ERROR, "Error Getting the system path");
    return false;
  }

  _systemPath = path;

  if (fnkdat("assets", path, sizeof(path), FNKDAT_VAR | FNKDAT_DATA | FNKDAT_CREAT) != 0)
  {
    _logger->printf(RETRO_LOG_ERROR, "Error getting the core assets path");
    return false;
  }

  _coreAssetsPath = path;

  if (fnkdat("saves", path, sizeof(path), FNKDAT_USER | FNKDAT_CREAT) != 0)
  {
    _logger->printf(RETRO_LOG_ERROR, "Error getting the saves path");
    return false;
  }

  _savePath = path;

  _updated = false;
  _opened = true;
  return true;
}

void Config::destroy()
{
  fnkdat(NULL, 0, 0, FNKDAT_UNINIT);
}

void Config::reset()
{
  _variables.clear();
}

void Config::draw()
{
  if (ImGui::Begin(ICON_FA_WRENCH " Configuration", &_opened))
  {
    for (auto it = _variables.begin(); it != _variables.end(); ++it)
    {
      Variable& var = *it;

      struct Getter
      {
        static bool get(void* data, int idx, const char** out_text)
        {
          const Variable* var = (Variable*)data;

          if ((size_t)idx < var->_options.size())
          {
            *out_text = var->_options[idx].c_str();
            return true;
          }

          return false;
        }
      };

      int old = var._selected;
      ImGui::Combo(var._name.c_str(), &var._selected, Getter::get, (void*)&var, var._options.size());
      _updated = _updated || old != var._selected;
    }
  }

  ImGui::End();
}

const char* Config::getCoreAssetsDirectory()
{
  return _coreAssetsPath.c_str();
}

const char* Config::getSaveDirectory()
{
  return _savePath.c_str();
}

const char* Config::getSystemPath()
{
  return _systemPath.c_str();
}

void Config::setVariables(const struct retro_variable* variables, unsigned count)
{
  for (unsigned i = 0; i < count; variables++, i++)
  {
    Variable var;
    var._key = variables->key;

    const char* aux = strchr(variables->value, ';');
    while (isspace(*++aux)) /* nothing */;

    var._name = std::string(variables->value, aux - variables->value );

    while (isspace(*aux))
    {
      aux++;
    }

    for (;;)
    {
      const char* pipe = strchr(aux, '|');

      if (pipe != NULL)
      {
        var._options.push_back(std::string(aux, pipe - aux));
        aux = pipe + 1;
      }
      else
      {
        var._options.push_back(std::string(aux));
        break;
      }
    }

    var._selected = 0;
    _variables.push_back(var);
  }
}

bool Config::varUpdated()
{
  bool updated = _updated;
  _updated = false;
  return updated;
}

const char* Config::getVariable(const char* variable)
{
  for (auto it = _variables.begin(); it != _variables.end(); ++it)
  {
    const Variable& var = *it;

    if (var._key == variable)
    {
      return var._options[ var._selected ].c_str();
    }
  }

  _logger->printf(RETRO_LOG_ERROR, "Unknown variable %s", variable);
  return NULL;
}

bool Video::init(libretro::LoggerComponent* logger)
{
  _logger = logger;
  _texture = 0;
  _opened = true;
  _width = _height = 0;
  return true;
}

void Video::destroy()
{
  if (_texture != 0)
  {
    glDeleteTextures(1, &_texture);
  }
}

void Video::reset()
{
}

void Video::draw()
{
  if (ImGui::Begin(ICON_FA_DESKTOP " Video", &_opened))
  {
    if (_texture != 0)
    {
      ImVec2 min = ImGui::GetWindowContentRegionMin();
      ImVec2 max = ImGui::GetWindowContentRegionMax();

      float height = max.y - min.y;
      float width = height * _aspect;

      if (width > max.x - min.x)
      {
        width = max.x - min.x;
        height = width / _aspect;
      }

      ImVec2 size = ImVec2(width, height);
      ImVec2 uv0 = ImVec2(0.0f, 0.0f);

      ImVec2 uv1 = ImVec2(
        (float)_width / _textureWidth,
        (float)_height / _textureHeight
      );

      ImGui::Image((ImTextureID)(uintptr_t)_texture, size, uv0, uv1);
    }
  }

  ImGui::End();
}

bool Video::setGeometry(unsigned width, unsigned height, float aspect, enum retro_pixel_format pixelFormat, bool needsHardwareRender)
{
  // TODO implement an OpenGL renderer
  (void)needsHardwareRender;

  if (_texture != 0)
  {
    glDeleteTextures(1, &_texture);
  }

  GLint previous_texture;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
  
  glGenTextures(1, &_texture);
  glBindTexture(GL_TEXTURE_2D, _texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  
  switch (pixelFormat)
  {
  case RETRO_PIXEL_FORMAT_XRGB8888:
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    break;
    
  case RETRO_PIXEL_FORMAT_RGB565:
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
    break;
    
  case RETRO_PIXEL_FORMAT_0RGB1555:
  default:
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
    break;
  }

  glBindTexture(GL_TEXTURE_2D, previous_texture);
  _textureWidth = width;
  _textureHeight = height;
  _pixelFormat = pixelFormat;
  _aspect = aspect;

  _logger->printf(RETRO_LOG_DEBUG, "Geometry set to %u x %u (1:%f)", width, height, aspect);

  return true;
}

void Video::refresh(const void* data, unsigned width, unsigned height, size_t pitch)
{
  GLint previous_texture;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
  
  glBindTexture(GL_TEXTURE_2D, _texture);
  uint8_t* p = (uint8_t*)data;

  GLenum type;
  
  switch (_pixelFormat)
  {
  case RETRO_PIXEL_FORMAT_XRGB8888:
    {
      uint32_t* q = (uint32_t*)alloca(width * 4);

      if (q != NULL)
      {
        for (unsigned y = 0; y < height; y++)
        {
          uint32_t* r = q;
          uint32_t* s = (uint32_t*)p;

          for (unsigned x = 0; x < width; x++)
          {
            uint32_t color = *s++;
            uint32_t red   = (color >> 16) & 255;
            uint32_t green = (color >> 8) & 255;
            uint32_t blue  = color & 255;
            *r++ = 0xff000000UL | blue << 16 | green << 8 | red;
          }

          glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void*)q);
          p += pitch;
        }
      }
    }
    
    goto end;
    
  case RETRO_PIXEL_FORMAT_RGB565:
    type = GL_UNSIGNED_SHORT_5_6_5;
    break;
    
  case RETRO_PIXEL_FORMAT_0RGB1555:
  default:
    type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
    break;
  }

  for (unsigned y = 0; y < height; y++)
  {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1, GL_RGB, type, (void*)p);
    p += pitch;
  }

end:
  glBindTexture(GL_TEXTURE_2D, previous_texture);

  if (width != _width || height != _height)
  {
    _width = width;
    _height = height;

    _logger->printf(RETRO_LOG_DEBUG, "Video refreshed with geometry %u x %u", width, height);
  }
}

void Video::showMessage(const char* msg, unsigned frames)
{
  _logger->printf(RETRO_LOG_INFO, "OSD message (%u): %s", frames, msg);
}

bool Audio::init(libretro::LoggerComponent* logger, unsigned sample_rate, Fifo* fifo)
{
  _opened = true;
  _mute = false;
  _scale = 1.0f;
  _samples = NULL;

  _coreRate = 0;
  _resampler = NULL;

  _logger = logger;
  _sampleRate = sample_rate;

  _fifo = fifo;
  return true;
}

void Audio::destroy()
{
  if (_resampler != NULL)
  {
    speex_resampler_destroy(_resampler);
  }
}

void Audio::reset()
{
}

void Audio::draw()
{
  if (ImGui::Begin(ICON_FA_VOLUME_UP " Audio", &_opened))
  {
    ImGui::Checkbox("Mute", &_mute);

    struct Getter
    {
      static float left(void* data, int idx)
      {
        Audio* self = (Audio*)data;
        return self->_samples[idx * 2] * self->_scale;
      }

      static float right(void* data, int idx)
      {
        Audio* self = (Audio*)data;
        return self->_samples[idx * 2 + 1] * self->_scale;
      }

      static float zero(void* data, int idx)
      {
        return 0.0f;
      }
    };

    ImVec2 max = ImGui::GetContentRegionAvail();
    max.y -= ImGui::GetItemsLineHeightWithSpacing();

    if (max.y > 0.0f)
    {
      max.x /= 2;

      if (_samples != NULL)
      {
        ImGui::PlotLines("", Getter::left, this, _frames, 0, NULL, -32768.0f, 32767.0f, max);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::PlotLines("", Getter::right, this, _frames, 0, NULL, -32768.0f, 32767.0f, max);
      }
      else
      {
        ImGui::PlotLines("", Getter::zero, NULL, 2, 0, NULL, -32768.0f, 32767.0f, max);
        ImGui::SameLine( 0.0f, 0.0f );
        ImGui::PlotLines("", Getter::zero, NULL, 2, 0, NULL, -32768.0f, 32767.0f, max);
      }
    }

    ImGui::PushItemWidth(-1.0f);
    ImGui::SliderFloat("##empty", &_scale, 1.0f, 3.0f, "Scale = %.3f");
    ImGui::PopItemWidth();

    _samples = NULL;
  }

  ImGui::End();
}

bool Audio::setRate(double rate)
{
  if (_resampler != NULL)
  {
    speex_resampler_destroy(_resampler);
    _resampler = NULL;
  }

  _coreRate = floor(rate + 0.5);

  if (_coreRate != _sampleRate)
  {
    int error;
    _resampler = speex_resampler_init(2, _coreRate, _sampleRate, SPEEX_RESAMPLER_QUALITY_DEFAULT, &error);

    if (_resampler == NULL)
    {
      _logger->printf(RETRO_LOG_ERROR, "Error creating resampler: %s", speex_resampler_strerror(error));
    }
    else
    {
      _logger->printf(RETRO_LOG_INFO, "Resampler initialized to convert from %u to %u", _coreRate, _sampleRate);
    }
  }
  else
  {
    _logger->printf(RETRO_LOG_INFO, "Frequencies are equal, resampler will copy samples unchanged");
  }

  return true;
}

void Audio::mix(const int16_t* samples, size_t frames)
{
  _samples = samples;
  _frames = frames;

  spx_uint32_t in_len = frames * 2;
  spx_uint32_t out_len = (spx_uint32_t)(in_len * _sampleRate / _coreRate);

  if ((out_len & 1) != 0)
  {
    out_len++;
  }

  int16_t* output = (int16_t*)alloca(out_len * 2);

  if (output == NULL)
  {
    return;
  }

  if (_mute)
  {
    memset(output, 0, out_len * 2);
  }
  else if (_resampler != NULL)
  {
    int error = speex_resampler_process_int(_resampler, 0, samples, &in_len, output, &out_len);

    if (error != RESAMPLER_ERR_SUCCESS)
    {
      _logger->printf(RETRO_LOG_ERROR, "Error resampling buffer: %s", speex_resampler_strerror(error));
    }
  }
  else
  {
    memcpy(output, samples, out_len * 2);
  }

  size_t size = out_len * 2;
  
again:
  size_t avail = _fifo->free();
  
  if (size > avail)
  {
    SDL_Delay(1);
    goto again;
  }
  
  _fifo->write(output, size);
}

bool Input::init(libretro::LoggerComponent* logger)
{
  GLint previous_texture;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
  
  glGenTextures(1, &_texture);
  glBindTexture(GL_TEXTURE_2D, _texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 768, 192, 0, GL_RGBA, GL_UNSIGNED_BYTE, s_controller);

  glBindTexture(GL_TEXTURE_2D, previous_texture);

  _logger = logger;
  _opened = true;
  return true;
}

void Input::destroy()
{
  glDeleteTextures(1, &_texture);
}

void Input::reset()
{
  _descriptors.clear();
  _controllers.clear();

  for (auto it = _pads.begin(); it != _pads.end(); ++it)
  {
    Pad* pad = &it->second;

    pad->_port = 0;
    pad->_devId = RETRO_DEVICE_NONE;
  }
}

void Input::draw()
{
  if (ImGui::Begin(ICON_FA_GAMEPAD " Input", &_opened))
  {
    unsigned count = 1;

    for (auto it = _pads.begin(); it != _pads.end(); ++it, count++)
    {
      Pad* pad = &it->second;

      char label[512];
      snprintf(label, sizeof(label), "%s (%u)", pad->_controllerName, count);

      if (ImGui::CollapsingHeader(label))
      {
        char id[32];
        snprintf(id, sizeof(id), "%p", pad);
        ImGui::Columns(2, id);

        {
          ImVec2 pos = ImGui::GetCursorPos();
          drawPad(17);

          for (unsigned button = 0; button < 16; button++)
          {
            if (pad->_state[button])
            {
              ImGui::SetCursorPos(pos);
              drawPad(button);
            }
          }
        }

        ImGui::NextColumn();

        {
          char labels[512];
          char* aux = labels;
          unsigned i = 1;

          aux += snprintf(aux, sizeof(labels) - (aux - labels), "Disconnected") + 1;

          if (_controllers.size() != 0)
          {
            for (auto it = _controllers.begin(); it != _controllers.end(); ++it, i++)
            {
              Controller* ctrl = &*it;
              
              for (auto it2 = ctrl->_types.begin(); it2 != ctrl->_types.end(); ++it2)
              {
                ControllerType* type = &*it2;

                if ((type->_id & RETRO_DEVICE_MASK) == RETRO_DEVICE_JOYPAD)
                {
                  aux += snprintf(aux, sizeof(labels) - (aux - labels), "Connected to port %u", i) + 1;
                  break;
                }
              }
            }
          }
          else
          {
            // No ports were specified, use the input descriptors to see how many ports we have
            uint64_t ports = 0;

            for (auto it = _descriptors.begin(); it != _descriptors.end(); ++it)
            {
              Descriptor* desc = &*it;

              if (desc->_port < 64)
              {
                ports |= 1 << desc->_port;
              }
            }

            for (unsigned i = 0; i < 64; i++)
            {
              if (ports & (1ULL << i))
              {
                aux += snprintf(aux, sizeof(labels) - (aux - labels), "Connect to port %u", i + 1) + 1;
              }
            }
          }

          *aux = 0;

          ImGui::PushItemWidth(-1.0f);

          char label[64];
          snprintf(label, sizeof(label), "##port%p", pad);

          ImGui::Combo(label, &pad->_port, labels);
          ImGui::PopItemWidth();
        }

        {
          char labels[512];
          unsigned ids[32];
          char* aux = labels;
          int count = 0;
          int selected = 0;

          aux += snprintf(aux, sizeof(labels) - (aux - labels), "None") + 1;
          ids[count++] = RETRO_DEVICE_NONE;

          if (_controllers.size() != 0)
          {
            if (pad->_port > 0 && (size_t)pad->_port <= _controllers.size())
            {
              Controller* ctrl = &_controllers[pad->_port - 1];

              for (auto it = ctrl->_types.begin(); it != ctrl->_types.end(); ++it)
              {
                ControllerType* type = &*it;

                if ((type->_id & RETRO_DEVICE_MASK) == RETRO_DEVICE_JOYPAD)
                {
                  if (type->_id == pad->_devId)
                  {
                    selected = count;
                  }

                  aux += snprintf(aux, sizeof(labels) - (aux - labels), "%s", type->_description.c_str()) + 1;
                  ids[count++] = type->_id;
                }
              }
            }
          }
          else
          {
            // No ports were specified, add the default RetroPad controller if the port is valid

            if (pad->_port != 0)
            {
              aux += snprintf(aux, sizeof(labels) - (aux - labels), "RetroPad") + 1;

              if (pad->_devId == RETRO_DEVICE_JOYPAD)
              {
                selected = 1;
              }
            }
          }

          *aux = 0;

          ImGui::PushItemWidth(-1.0f);

          char label[64];
          snprintf(label, sizeof(label), "##device%p", pad);

          int old = selected;
          ImGui::Combo(label, &selected, labels);
          _updated = _updated || old != selected;

          if (_controllers.size() != 0)
          {
            pad->_devId = ids[selected];
          }
          else
          {
            pad->_devId = selected == 0 ? RETRO_DEVICE_NONE : RETRO_DEVICE_JOYPAD;
          }

          ImGui::PopItemWidth();
        }

        {
          char label[64];
          snprintf(label, sizeof(label), "##sensitivity%p", pad);

          ImGui::PushItemWidth(-1.0f);
          ImGui::SliderFloat(label, &pad->_sensitivity, 0.0f, 1.0f, "Sensitivity %.3f");
          ImGui::PopItemWidth();
        }

        ImGui::Columns(1);
      }
    }
  }

  ImGui::End();
}

void Input::drawPad(unsigned button)
{
  unsigned y = button / 6;
  unsigned x = button - y * 6;

  float xx = x * 128.0f;
  float yy = y * 64.0f;

  ImVec2 size = ImVec2(113.0f, 63.0f);
  ImVec2 uv0 = ImVec2(xx / 768.0f, yy / 192.0f);
  ImVec2 uv1 = ImVec2((xx + 128.0f) / 768.0f, (yy + 64.0f) / 192.0f);

  ImGui::Image((ImTextureID)(uintptr_t)_texture, size, uv0, uv1);
}

void Input::addController(int which)
{
  if (SDL_IsGameController(which))
  {
    Pad pad;

    pad._controller = SDL_GameControllerOpen(which);

    if (pad._controller == NULL)
    {
      _logger->printf(RETRO_LOG_ERROR, "Error opening the controller: %s", SDL_GetError());
      return;
    }

    pad._joystick = SDL_GameControllerGetJoystick(pad._controller);

    if (pad._joystick == NULL)
    {
      _logger->printf(RETRO_LOG_ERROR, "Error getting the joystick: %s", SDL_GetError());
      SDL_GameControllerClose(pad._controller);
      return;
    }

    pad._id = SDL_JoystickInstanceID(pad._joystick);

    if (_pads.find(pad._id) == _pads.end())
    {
      pad._controllerName = SDL_GameControllerName(pad._controller);
      pad._joystickName = SDL_JoystickName(pad._joystick);
      pad._lastDir[0] = pad._lastDir[1] = pad._lastDir[2] =
      pad._lastDir[3] = pad._lastDir[4] = pad._lastDir[5] = -1;
      pad._sensitivity = 0.5f;
      pad._port = 0;
      pad._devId = RETRO_DEVICE_NONE;
      memset(pad._state, 0, sizeof(pad._state));

      _pads.insert(std::make_pair(pad._id, pad));
      _logger->printf(RETRO_LOG_INFO, "Controller %s (%s) added", pad._controllerName, pad._joystickName);
    }
    else
    {
      SDL_GameControllerClose(pad._controller);
    }
  }
}

void Input::processEvent(const SDL_Event* event)
{
  switch (event->type)
  {
  case SDL_CONTROLLERDEVICEADDED:
    addController(event);
    break;

  case SDL_CONTROLLERDEVICEREMOVED:
    removeController(event);
    break;

  case SDL_CONTROLLERBUTTONUP:
  case SDL_CONTROLLERBUTTONDOWN:
    controllerButton(event);
    break;

  case SDL_CONTROLLERAXISMOTION:
    controllerAxis(event);
    break;
  }
}

void Input::setInputDescriptors(const struct retro_input_descriptor* descs, unsigned count)
{
  for (unsigned i = 0; i < count; descs++, i++)
  {
    Descriptor desc;
    desc._port = descs->port;
    desc._device = descs->device;
    desc._index = descs->index;
    desc._id = descs->id;
    desc._description = descs->description;

    _descriptors.push_back(desc);
  }
}

void Input::setControllerInfo(const struct retro_controller_info* info, unsigned count)
{
  for (unsigned i = 0; i < count; info++, i++)
  {
    Controller ctrl;

    for (unsigned j = 0; j < info->num_types; j++)
    {
      ControllerType type;
      type._description = info->types[j].desc;
      type._id = info->types[j].id;

      ctrl._types.push_back(type);
    }

    _controllers.push_back(ctrl);
  }
}

bool Input::ctrlUpdated()
{
  bool updated = _updated;
  _updated = false;
  return updated;
}

unsigned Input::getController(unsigned port)
{
  port++;

  for (auto it = _pads.begin(); it != _pads.end(); ++it)
  {
    Pad* pad = &it->second;

    if (pad->_port == (int)port)
    {
      return pad->_devId;
    }
  }

  // The controller was removed
  return RETRO_DEVICE_NONE;
}

void Input::poll()
{
  // Events are polled in the main event loop, and arrive in this class via
  // the processEvent method
}

int16_t Input::read(unsigned port, unsigned device, unsigned index, unsigned id)
{
  (void)index;

  port++;

  for (auto it = _pads.begin(); it != _pads.end(); ++it)
  {
    Pad* pad = &it->second;

    if (pad->_port == (int)port && pad->_devId == device)
    {
      return pad->_state[id] ? 32767 : 0;
    }
  }

  return 0;
}

void Input::addController(const SDL_Event* event)
{
  addController(event->cdevice.which);
}

void Input::removeController(const SDL_Event* event)
{
  auto it = _pads.find(event->cdevice.which);

  if (it != _pads.end())
  {
    Pad* pad = &it->second;
    _logger->printf(RETRO_LOG_INFO, "Controller %s (%s) removed", pad->_controllerName, pad->_joystickName);
    SDL_GameControllerClose(pad->_controller);
    _pads.erase(it);

    // Flag a pending update so the core will receive an event for this removal
    _updated = true;
  }
}

void Input::controllerButton(const SDL_Event* event)
{
  auto it = _pads.find(event->cbutton.which);

  if (it != _pads.end())
  {
    Pad* pad = &it->second;
    unsigned button;

    switch (event->cbutton.button)
    {
    case SDL_CONTROLLER_BUTTON_A:             button = RETRO_DEVICE_ID_JOYPAD_B; break;
    case SDL_CONTROLLER_BUTTON_B:             button = RETRO_DEVICE_ID_JOYPAD_A; break;
    case SDL_CONTROLLER_BUTTON_X:             button = RETRO_DEVICE_ID_JOYPAD_Y; break;
    case SDL_CONTROLLER_BUTTON_Y:             button = RETRO_DEVICE_ID_JOYPAD_X; break;
    case SDL_CONTROLLER_BUTTON_BACK:          button = RETRO_DEVICE_ID_JOYPAD_SELECT; break;
    case SDL_CONTROLLER_BUTTON_START:         button = RETRO_DEVICE_ID_JOYPAD_START; break;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:     button = RETRO_DEVICE_ID_JOYPAD_L3; break;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    button = RETRO_DEVICE_ID_JOYPAD_R3; break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  button = RETRO_DEVICE_ID_JOYPAD_L; break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: button = RETRO_DEVICE_ID_JOYPAD_R; break;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:       button = RETRO_DEVICE_ID_JOYPAD_UP; break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     button = RETRO_DEVICE_ID_JOYPAD_DOWN; break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     button = RETRO_DEVICE_ID_JOYPAD_LEFT; break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    button = RETRO_DEVICE_ID_JOYPAD_RIGHT; break;
    case SDL_CONTROLLER_BUTTON_GUIDE:         // fallthrough
    default:                                  return;
    }

    pad->_state[button] = event->cbutton.state == SDL_PRESSED;
  }
}

void Input::controllerAxis(const SDL_Event* event)
{
  auto it = _pads.find(event->caxis.which);

  if (it != _pads.end())
  {
    Pad* pad = &it->second;
    int threshold = 32767 * pad->_sensitivity;
    int positive, negative;
    int button;
    int *last_dir;

    switch (event->caxis.axis)
    {
    case SDL_CONTROLLER_AXIS_LEFTX:
    case SDL_CONTROLLER_AXIS_LEFTY:
    case SDL_CONTROLLER_AXIS_RIGHTX:
    case SDL_CONTROLLER_AXIS_RIGHTY:
      switch (event->caxis.axis)
      {
      case SDL_CONTROLLER_AXIS_LEFTX:
        positive = RETRO_DEVICE_ID_JOYPAD_RIGHT;
        negative = RETRO_DEVICE_ID_JOYPAD_LEFT;
        last_dir = pad->_lastDir + 0;
        break;

      case SDL_CONTROLLER_AXIS_LEFTY:
        positive = RETRO_DEVICE_ID_JOYPAD_DOWN;
        negative = RETRO_DEVICE_ID_JOYPAD_UP;
        last_dir = pad->_lastDir + 1;
        break;

      case SDL_CONTROLLER_AXIS_RIGHTX:
        positive = RETRO_DEVICE_ID_JOYPAD_RIGHT;
        negative = RETRO_DEVICE_ID_JOYPAD_LEFT;
        last_dir = pad->_lastDir + 2;
        break;

      case SDL_CONTROLLER_AXIS_RIGHTY:
        positive = RETRO_DEVICE_ID_JOYPAD_DOWN;
        negative = RETRO_DEVICE_ID_JOYPAD_UP;
        last_dir = pad->_lastDir + 3;
        break;
      }

      if (event->caxis.value < -threshold)
      {
        button = negative;
      }
      else if (event->caxis.value > threshold)
      {
        button = positive;
      }
      else
      {
        button = -1;
      }

      break;

    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
      if (event->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
      {
        button = RETRO_DEVICE_ID_JOYPAD_L2;
        last_dir = pad->_lastDir + 4;
      }
      else
      {
        button = RETRO_DEVICE_ID_JOYPAD_R2;
        last_dir = pad->_lastDir + 5;
      }

      break;

    default:
      return;
    }

    if (*last_dir != -1)
    {
      pad->_state[*last_dir] = false;
    }

    if (event->caxis.value < -threshold || event->caxis.value > threshold)
    {
      pad->_state[button] = true;
    }

    *last_dir = button;
  }
}

bool Loader::init(libretro::LoggerComponent* logger)
{
  _logger = logger;
  return true;
}

void Loader::destroy()
{
}

void Loader::reset()
{
}

void* Loader::load(size_t* size, const char* path)
{
  struct stat statbuf;

  if (stat(path, &statbuf) != 0)
  {
    _logger->printf(RETRO_LOG_ERROR, "Error getting content info: %s", strerror(errno));
    return NULL;
  }

  void *data = malloc(statbuf.st_size);

  if (data == NULL)
  {
    _logger->printf(RETRO_LOG_ERROR, "Out of memory allocating %u bytes", statbuf.st_size);
    return NULL;
  }

  FILE* file = fopen(path, "rb");

  if (file == NULL)
  {
    _logger->printf(RETRO_LOG_ERROR, "Error opening content: %s", strerror(errno));
    free(data);
    return NULL;
  }

  size_t numread = fread(data, 1, statbuf.st_size, file);

  if (numread < 0 || numread != (size_t)statbuf.st_size)
  {
    _logger->printf(RETRO_LOG_ERROR, "Error reading content: %s", strerror(errno));
    fclose(file);
    free(data);
    return NULL;
  }

  fclose(file);

  *size = statbuf.st_size;
  return data;
}

void Loader::free(void* data)
{
  ::free(data);
}