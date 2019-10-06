/*
The MIT License (MIT)

Copyright (c) 2016 Andre Leiradella

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

/**
 * Include cstdarg for va_list usage.
 */
#include <cstdarg>

#include "libretropp.h"

namespace libretro {
  /**
   * A logger component for Core instances.
   */
  class LoggerComponent   {
  public:
    virtual void vprintf(enum retro_log_level level, const char* fmt, va_list args) = 0;

    inline void printf(enum retro_log_level level, const char* fmt, ...) {
      va_list args;
      va_start(args, fmt);
      vprintf(level, fmt, args);
      va_end(args);
    }
  };

  /**
   * A component that returns configuration values for CoreWrap instances.
   */
  class ConfigComponent {
  public:
    virtual std::string const& getCoreAssetsDirectory() = 0;
    virtual std::string const& getSaveDirectory() = 0;
    virtual std::string const& getSystemPath() = 0;

    virtual void setVariables(std::vector<Variable> const& variables) = 0;
    virtual bool varUpdated() = 0;
    virtual std::string const& getVariable(std::string const& variable) = 0;
  };

  /**
   * A Video component.
   */
  class VideoComponent {
  public:
    virtual bool setGeometry(unsigned width, unsigned height, float aspect, enum retro_pixel_format pixelFormat) = 0;
    virtual void refresh(const void* data, unsigned width, unsigned height, size_t pitch) = 0;

    virtual uintptr_t getCurrentFramebuffer() = 0;

    virtual void showMessage(std::string const& msg, unsigned frames) = 0;
  };

  /**
   * An audio component.
   */
  class AudioComponent {
  public:
    virtual bool setRate(double rate) = 0;
    virtual void mix(const int16_t* samples, size_t frames) = 0;
  };

  /**
   * A component that provides input state to CoreWrap instances.
   */
  class InputComponent {
  public:
    virtual void setInputDescriptors(std::vector<InputDescriptor> const& descs) = 0;

    virtual void     setControllerInfo(std::vector<ControllerInfo> const& info) = 0;
    virtual bool     ctrlUpdated() = 0;
    virtual unsigned getController(unsigned port) = 0;

    virtual void    poll() = 0;
    virtual int16_t read(unsigned port, unsigned device, unsigned index, unsigned id) = 0;
  };

  /**
   * A component responsible for loading content from the file system.
   */
  class LoaderComponent {
  public:
    virtual void* load(size_t* size, std::string const& path) = 0;
    virtual void  free(void* data) = 0;
  };
}
