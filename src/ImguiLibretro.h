#pragma once

#include "libretro/Core.h"

#include "imguiext/imguial_log.h"
#include "speex/speex_resampler.h"

#include "json.hpp"

#include <stdarg.h>
#include <string>
#include <vector>
#include <map>

#include <SDL.h>
#include <SDL_opengl.h>

using json = nlohmann::json;

class Fifo
{
public:
  bool init(libretro::LoggerComponent* logger, size_t size);
  void destroy();
  void reset();

  void read(void* data, size_t size);
  void write(const void* data, size_t size);

  size_t occupied();
  size_t free();

protected:

  libretro::LoggerComponent* _logger;

  SDL_mutex* _mutex;
  uint8_t*   _buffer;
  size_t     _size;
  size_t     _avail;
  size_t     _first;
  size_t     _last;
};

class Logger: public libretro::LoggerComponent
{
public:
  bool init();
  void destroy();
  void reset();
  void draw();

  virtual void vprintf(enum retro_log_level level, const char* fmt, va_list args) override;

protected:
  ImGuiAl::Log _logger;
};

class Config: public libretro::ConfigComponent
{
public:
  bool init(libretro::LoggerComponent* logger, json* json);
  void destroy();
  void reset();
  void draw();

  virtual const char* getCoreAssetsDirectory() override;
  virtual const char* getSaveDirectory() override;
  virtual const char* getSystemPath() override;

  virtual void setVariables(const struct retro_variable* variables, unsigned count) override;
  virtual bool varUpdated() override;
  virtual const char* getVariable(const char* variable) override;

protected:
  struct Variable
  {
    std::string _key;
    std::string _name;
    std::string _value;
    int         _selected;
    std::vector<std::string> _options;
  };

  libretro::LoggerComponent* _logger;
  bool           _opened;
  std::string    _systemPath;
  std::string    _coreAssetsPath;
  std::string    _savePath;

  json* _json;

  bool                  _updated;
  std::vector<Variable> _variables;
};

class Video: public libretro::VideoComponent
{
public:
  bool init(libretro::LoggerComponent* logger);
  void destroy();
  void reset();
  void draw();

  virtual bool setGeometry(unsigned width, unsigned height, float aspect, enum retro_pixel_format pixelFormat, bool needsHardwareRender) override;
  virtual void refresh(const void* data, unsigned width, unsigned height, size_t pitch) override;

  virtual bool                 supportsContext(enum retro_hw_context_type type) override;
  virtual uintptr_t            getCurrentFramebuffer() override;
  virtual retro_proc_address_t getProcAddress(const char* symbol) override;

  virtual void showMessage(const char* msg, unsigned frames) override;

protected:
  libretro::LoggerComponent* _logger;
  GLuint                  _texture;
  unsigned                _textureWidth;
  unsigned                _textureHeight;
  enum retro_pixel_format _pixelFormat;

  bool     _opened;
  unsigned _width;
  unsigned _height;
  float    _aspect;
};

class Audio: public libretro::AudioComponent
{
public:
  bool init(libretro::LoggerComponent* logger, unsigned sample_rate, Fifo* fifo);
  void destroy();
  void reset();
  void draw();

  virtual bool setRate(double rate) override;
  virtual void mix(const int16_t* samples, size_t frames) override;

protected:
  libretro::LoggerComponent* _logger;

  unsigned _sampleRate;
  unsigned _coreRate;
  SpeexResamplerState* _resampler;

  Fifo* _fifo;

  bool           _opened;
  bool           _mute;
  float          _scale;
  const int16_t* _samples;
  size_t         _frames;
};

class Input: public libretro::InputComponent
{
public:
  bool init(libretro::LoggerComponent* logger, json* json);
  void destroy();
  void reset();
  void draw();

  void addController(int which);
  void processEvent(const SDL_Event* event);

  virtual void setInputDescriptors(const struct retro_input_descriptor* descs, unsigned count) override;

  virtual void     setControllerInfo(const struct retro_controller_info* info, unsigned count) override;
  virtual bool     ctrlUpdated() override;
  virtual unsigned getController(unsigned port) override;

  virtual void    poll() override;
  virtual int16_t read(unsigned port, unsigned device, unsigned index, unsigned id) override;

protected:
  struct Pad
  {
    SDL_JoystickID      _id;
    SDL_GameController* _controller;
    const char*         _controllerName;
    SDL_Joystick*       _joystick;
    const char*         _joystickName;
    int                 _lastDir[6];
    bool                _state[16];
    float               _sensitivity;

    int      _port;
    unsigned _devId;
  };

  struct Descriptor
  {
    unsigned _port;
    unsigned _device;
    unsigned _index;
    unsigned _id;

    std::string _description;
  };

  struct ControllerType
  {
    std::string _description;
    unsigned    _id;
  };

  struct Controller
  {
    std::vector<ControllerType> _types;
  };

  void drawPad(unsigned button);
  void addController(const SDL_Event* event);
  void removeController(const SDL_Event* event);
  void controllerButton(const SDL_Event* event);
  void controllerAxis(const SDL_Event* event);

  libretro::LoggerComponent* _logger;

  bool   _opened;
  GLuint _texture;
  bool   _updated;

  json* _json;

  std::map<SDL_JoystickID, Pad> _pads;
  std::vector<Descriptor>       _descriptors;
  std::vector<Controller>       _controllers;
};

class Loader: public libretro::LoaderComponent
{
public:
  bool init(libretro::LoggerComponent* logger);
  void destroy();
  void reset();

  virtual void* load(size_t* size, const char* path) override;
  virtual void  free(void* data) override;

protected:
  libretro::LoggerComponent* _logger;
};

template<size_t N>
class Allocator: public libretro::AllocatorComponent
{
public:
  inline bool init(libretro::LoggerComponent* logger)
  {
    _logger = logger;
    _head = 0;
    return true;
  }

  inline void destroy() {}

  virtual void  reset() override
  {
    _head = 0;
  }

  virtual void* allocate(size_t alignment, size_t numBytes) override
  {
    size_t align_m1 = alignment - 1;
    size_t ptr = (_head + align_m1) & ~align_m1;

    if (numBytes <= (kBufferSize - ptr))
    {
      _head = ptr + numBytes;
      return (void*)(_buffer + ptr);
    }

    _logger->printf(RETRO_LOG_ERROR, "Out of memory allocating %u bytes", numBytes);
    return NULL;
  }

protected:
  enum
  {
    kBufferSize = N
  };

  libretro::LoggerComponent* _logger;
  uint8_t        _buffer[N];
  size_t         _head;
};
