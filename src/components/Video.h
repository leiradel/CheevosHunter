#pragma once

#include "libretro/Components.h"

#include <SDL.h>
#include <SDL_opengl.h>

class Video: public libretro::VideoComponent
{
public:
  bool init(libretro::LoggerComponent* logger);
  void destroy();
  void reset();
  void draw();

  virtual bool setGeometry(unsigned width, unsigned height, float aspect, enum retro_pixel_format pixelFormat) override;
  virtual void refresh(const void* data, unsigned width, unsigned height, size_t pitch) override;

  virtual uintptr_t getCurrentFramebuffer() override;

  virtual void showMessage(std::string const& msg, unsigned frames) override;

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
