/*
The MIT License (MIT)

Copyright (c) 2019 Andre Leiradella

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

#include <imgui.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

namespace ImGuiAl
{
  class Fifo {
  public:
    Fifo(void* const buffer, size_t const size);

    void reset();
    size_t size() const;
    size_t occupied() const;
    size_t available() const;

    size_t read(void* const data, size_t const count);
    size_t peek(size_t const pos, void* const data, size_t const count);
    size_t write(void const* const data, size_t const count);
    size_t skip(size_t const count);

  protected:
    void* _buffer;
    size_t const _size;
    size_t _available;
    size_t _first;
    size_t _last;
  };

  class Crt {
  public:
    struct CGA {
      enum : ImU32 {
        Black         = IM_COL32(0x00, 0x00, 0x00, 0xff),
        Blue          = IM_COL32(0x00, 0x00, 0xaa, 0xff),
        Green         = IM_COL32(0x00, 0xaa, 0x00, 0xff),
        Cyan          = IM_COL32(0x00, 0xaa, 0xaa, 0xff),
        Red           = IM_COL32(0xaa, 0x00, 0x00, 0xff),
        Magenta       = IM_COL32(0xaa, 0x00, 0xaa, 0xff),
        Brown         = IM_COL32(0xaa, 0x55, 0x00, 0xff),
        White         = IM_COL32(0xaa, 0xaa, 0xaa, 0xff),
        Gray          = IM_COL32(0x55, 0x55, 0x55, 0xff),
        BrightBlue    = IM_COL32(0x55, 0x55, 0xff, 0xff),
        BrightGreen   = IM_COL32(0x55, 0xff, 0x55, 0xff),
        BrightCyan    = IM_COL32(0x55, 0xff, 0xff, 0xff),
        BrightRed     = IM_COL32(0xff, 0x55, 0x55, 0xff),
        BrightMagenta = IM_COL32(0xff, 0x55, 0xff, 0xff),
        Yellow        = IM_COL32(0xff, 0xff, 0x55, 0xff),
        BrightWhite   = IM_COL32(0xff, 0xff, 0xff, 0xff)
      };
    };

    struct Info {
      ImU32 foregroundColor;
      unsigned length;
      unsigned metaData;
    };

    typedef bool (*Iterator)(Info const& header, char const* line, void* ud);

    Crt(void* const buffer, size_t const size);

    void setForegroundColor(ImU32 const color);
    void setMetaData(unsigned const meta_data);

    void printf(char const* const format, ...);
    void vprintf(char const* const format, va_list args);
    void scrollToBottom();
    void clear();

    void iterate(void* const ud, Iterator const iterator);
    void draw();

  protected:
    void draw(void* const ud, Iterator const filter);

    Fifo _fifo;
    ImU32 _foregroundColor;
    unsigned _metaData;
    bool _scrollToBottom;
  };

  class Log : protected Crt {
  public:
    typedef Crt::Info Info;

    enum class Level {
      kDebug,
      kInfo,
      kWarning,
      kError
    };

    Log(void* const buffer, size_t const buffer_size);
    
    void debug(char const* const format, ...);
    void info(char const* const format, ...);
    void warning(char const* const format, ...);
    void error(char const* const format, ...);

    void debug(char const* const format, va_list args);
    void info(char const* const format, va_list args);
    void warning(char const* const format, va_list args);
    void error(char const* const format, va_list args);

    void clear() { Crt::clear(); }
    void iterate(void* const ud, Iterator const iterator) { Crt::iterate(ud, iterator); }
    void scrollToBottom() { Crt::scrollToBottom(); }

    int draw();

    void setColor(Level const level, ImU32 const color);
    void setLabel(Level const level, char const* const label);
    void setCumulativeLabel(char const* const label);
    void setFilterLabel(char const* const label);
    void setFilterHeaderLabel(char const* const label);
    void setActions(char const* actions[]);

  protected:
    ImU32 _debugTextColor;
    ImU32 _debugButtonColor;
    ImU32 _debugButtonHoveredColor;

    ImU32 _infoTextColor;
    ImU32 _infoButtonColor;
    ImU32 _infoButtonHoveredColor;
    
    ImU32 _warningTextColor;
    ImU32 _warningButtonColor;
    ImU32 _warningButtonHoveredColor;
    
    ImU32 _errorTextColor;
    ImU32 _errorButtonColor;
    ImU32 _errorButtonHoveredColor;
    
    char const* _debugLabel;
    char const* _infoLabel;
    char const* _warningLabel;
    char const* _errorLabel;

    char const* _cumulativeLabel;
    char const* _filterLabel;
    char const* _filterHeaderLabel;

    bool _showFilters;
    char const* const* _actions;

    Level _level;
    bool _cumulative;
    ImGuiTextFilter _filter;
  };

  class Terminal : protected Crt {
  public:
    typedef Crt::Info Info;

    Terminal(void* const buffer, size_t const buffer_size, void* const cmd_buf, size_t const cmd_size);

    void setForegroundColor(ImU32 const color) { Crt::setForegroundColor(color); }

    void printf(char const* const format, ...);
    void vprintf(char const* const format, va_list args) { Crt::vprintf(format, args); }
    void clear() { Crt::clear(); }

    void iterate(void* const ud, Iterator const iterator) { Crt::iterate(ud, iterator); }

    void draw();

    virtual void execute(char* command) = 0;
    /*virtual void onArrowUp(char* const command, size_t const max_length) = 0;
    virtual void onArrowDown(char* const command, size_t const max_length) = 0;
    virtual void onTab(char* const command, size_t const max_length) = 0;*/

  protected:
    char* _commandBuffer;
    size_t _cmdBufferSize;
  };

  template<size_t BUFFER_SIZE>
  class BufferedCrt : public Crt {
  public:
    BufferedCrt() : Crt(_buffer, BUFFER_SIZE) {}
  
  protected:
    uint8_t _buffer[BUFFER_SIZE];
  };

  template<size_t BUFFER_SIZE>
  class BufferedLog : public Log {
  public:
    BufferedLog() : Log(_buffer, BUFFER_SIZE) {}
  
  protected:
    uint8_t _buffer[BUFFER_SIZE];
  };

  template<size_t BUFFER_SIZE, size_t COMMAND_BUFFER_SIZE>
  class BufferedTerminal : public Terminal {
  public:
    BufferedTerminal() : Terminal(_buffer, BUFFER_SIZE, _commandBuffer, COMMAND_BUFFER_SIZE) {}
  
  protected:
    uint8_t _buffer[BUFFER_SIZE];
    uint8_t _commandBuffer[COMMAND_BUFFER_SIZE];
  };
}
