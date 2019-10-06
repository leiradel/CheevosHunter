#include "imguial_term.h"

#include <stdio.h>
#include <ctype.h>

ImGuiAl::Fifo::Fifo(void* const buffer, size_t const size) : _buffer(buffer), _size(size), _available(size), _first(0), _last(0) {}

void ImGuiAl::Fifo::reset() {
  _available = _size;
  _first = _last = 0;
}

size_t ImGuiAl::Fifo::size() const {
  return _size;
}

size_t ImGuiAl::Fifo::occupied() const {
  return _size - _available;
}

size_t ImGuiAl::Fifo::available() const {
  return _available;
}

size_t ImGuiAl::Fifo::read(void* const data, size_t const count) {
  size_t const num_read = peek(0, data, count);
  _first = (_first + num_read) % _size;
  _available += num_read;
  return num_read;
}

size_t ImGuiAl::Fifo::peek(size_t const pos, void* const data, size_t const count) {
  size_t const occupied = _size - _available - pos;
  size_t const first = (_first + pos) % _size;

  size_t const to_peek = count <= occupied ? count : occupied;

  size_t first_batch = to_peek;
  size_t second_batch = 0;

  if (first_batch > _size - first) {
    first_batch = _size - first;
    second_batch = to_peek - first_batch;
  }

  uint8_t const* const src = static_cast<uint8_t*>(_buffer) + first;
  memcpy(data, src, first_batch);
  memcpy(static_cast<uint8_t*>(data) + first_batch, _buffer, second_batch);
  
  return to_peek;
}

size_t ImGuiAl::Fifo::write(void const* const data, size_t const count) {
  size_t const to_write = count <= _available ? count : _available;
  size_t first_batch = to_write;
  size_t second_batch = 0;
  
  if (first_batch > _size - _last) {
    first_batch = _size - _last;
    second_batch = to_write - first_batch;
  }
  
  uint8_t* const dest = static_cast<uint8_t*>(_buffer) + _last;
  memcpy(dest, data, first_batch);
  memcpy(_buffer, static_cast<uint8_t const*>(data) + first_batch, second_batch);
  
  _last = (_last + to_write) % _size;
  _available -= to_write;
  return to_write;
}

size_t ImGuiAl::Fifo::skip(size_t const count) {
  size_t const to_skip = count <= (_size - _available) ? count : (_size - _available);
  _first = (_first + to_skip) % _size;
  _available += to_skip;
  return to_skip;
}

ImGuiAl::Crt::Crt(void* const buffer, size_t const size)
  : _fifo(buffer, size)
  , _foregroundColor(CGA::White)
  , _metaData(0)
  , _scrollToBottom(false) {}

void ImGuiAl::Crt::setForegroundColor(ImU32 const color) {
  _foregroundColor = color;
}

void ImGuiAl::Crt::setMetaData(unsigned const meta_data) {
  _metaData = meta_data;
}

void ImGuiAl::Crt::printf(char const* const format, ...) {
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}

void ImGuiAl::Crt::vprintf(char const* const format, va_list args) {
  char temp[256];
  char* line = temp;

  va_list args_copy;
  va_copy(args_copy, args);

  // TODO does vsnprintf ever return a negative value?
  size_t const needed = ::vsnprintf(line, sizeof(temp), format, args);
  size_t const length = needed < _fifo.size() - sizeof(Info) ? needed : _fifo.size() - sizeof(Info);

  if (length >= sizeof(temp)) {
    line = new char[length + 1];
    ::vsnprintf(line, length, format, args_copy);
  }

  va_end(args_copy);

  while (length + sizeof(Info) > _fifo.available()) {
    Info header;
    _fifo.read(&header, sizeof(header));
    _fifo.skip(header.length);
  }

  Info header;  
  header.foregroundColor = _foregroundColor;
  header.length = length;
  header.metaData = _metaData;
  
  _fifo.write(&header, sizeof(header));
  _fifo.write(line, length);

  if (line != temp) {
    delete[] line;
  }
}

void ImGuiAl::Crt::scrollToBottom() {
  _scrollToBottom = true;
}

void ImGuiAl::Crt::clear() {
  _fifo.reset();
}

void ImGuiAl::Crt::iterate(void* const ud, Iterator const iterator) {
  size_t const occupied = _fifo.occupied();
  size_t pos = 0;
  
  char temp[256];
  char* line = temp;
  size_t max_length = sizeof(temp);

  while (pos < occupied) {
    Info header;
    pos += _fifo.peek(pos, &header, sizeof(header));
    
    if (header.length >= max_length) {
      if (line != temp) {
        delete[] line;
      }

      max_length = header.length + 1;
      line = new char[max_length];
    }

    pos += _fifo.peek(pos, line, header.length);
    line[header.length] = 0;

    if (!iterator(header, line, ud)) {
      break;
    }
  }

  if (line != temp) {
    delete[] line;
  }
}

void ImGuiAl::Crt::draw() {
  draw(nullptr, nullptr);
}

void ImGuiAl::Crt::draw(void* const ud, Iterator const filter) {
  char id[64];
  snprintf(id, sizeof(id), "ImGuiAl::Crt@%p", this);
  
  ImGui::BeginChild(id, ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 1.0f));

  struct UdFilter {
    void* ud;
    Iterator filter;
  };

  UdFilter ud_filter = {ud, filter};
  
  iterate(reinterpret_cast<void*>(&ud_filter), [](Info const& header,
                                                  char const* const line,
                                                  void* const ud) -> bool {

    auto const ud_filter = reinterpret_cast<UdFilter*>(ud);
    
    if (ud_filter->filter == nullptr || ud_filter->filter(header, line, ud_filter->ud)) {
      ImGui::PushStyleColor(ImGuiCol_Text, header.foregroundColor);
      ImGui::TextUnformatted(line);
      ImGui::PopStyleColor();
    }
    
    return true;
  });
  
  if (_scrollToBottom) {
    ImGui::SetScrollHere();
    _scrollToBottom = false;
  }
  
  ImGui::PopStyleVar();
  ImGui::EndChild();
}

static ImU32 changeValue(ImU32 color, float const delta_value) {
  ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(color);

  float h, s, v;
  ImGui::ColorConvertRGBtoHSV(rgba.x, rgba.y, rgba.z, h, s, v);

  v += delta_value;

  if (v < 0.0f) {
    v = 0.0f;
  }
  else if (v > 1.0f) {
    v = 1.0f;
  }

  ImGui::ColorConvertHSVtoRGB(h, s, v + delta_value, rgba.x, rgba.y, rgba.z);
  return ImGui::ColorConvertFloat4ToU32(rgba);
}

ImGuiAl::Log::Log(void* const buffer, size_t const buffer_size)
  : Crt(buffer, buffer_size)
  , _debugLabel("Debug")
  , _infoLabel("Info")
  , _warningLabel("Warning")
  , _errorLabel("Error")
  , _cumulativeLabel("Cumulative")
  , _filterLabel("Filter (inc,-exc)")
  , _filterHeaderLabel(nullptr)
  , _showFilters(true)
  , _actions(nullptr)
  , _level(Level::kDebug)
  , _cumulative(true) {

  setColor(Level::kDebug, CGA::BrightBlue);
  setColor(Level::kInfo, CGA::BrightGreen);
  setColor(Level::kWarning, CGA::Yellow);
  setColor(Level::kError, CGA::BrightRed);
}

void ImGuiAl::Log::debug(char const* const format, ...) {
  va_list args;
  va_start(args, format);
  debug(format, args);
  va_end(args);
}

void ImGuiAl::Log::info(char const* const format, ...) {
  va_list args;
  va_start(args, format);
  info(format, args);
  va_end(args);
}

void ImGuiAl::Log::warning(char const* const format, ...) {
  va_list args;
  va_start(args, format);
  warning(format, args);
  va_end(args);
}

void ImGuiAl::Log::error(char const* const format, ...) {
  va_list args;
  va_start(args, format);
  error(format, args);
  va_end(args);
}

void ImGuiAl::Log::debug(char const* const format, va_list args) {
  setForegroundColor(_debugTextColor);
  setMetaData(static_cast<unsigned>(Level::kDebug));
  vprintf(format, args);
}

void ImGuiAl::Log::info(char const* const format, va_list args) {
  setForegroundColor(_infoTextColor);
  setMetaData(static_cast<unsigned>(Level::kInfo));
  vprintf(format, args);
}

void ImGuiAl::Log::warning(char const* const format, va_list args) {
  setForegroundColor(_warningTextColor);
  setMetaData(static_cast<unsigned>(Level::kWarning));
  vprintf(format, args);
}

void ImGuiAl::Log::error(char const* const format, va_list args) {
  setForegroundColor(_errorTextColor);
  setMetaData(static_cast<unsigned>(Level::kError));
  vprintf(format, args);
}

int ImGuiAl::Log::draw() {
  int action = 0;
  
  for (unsigned i = 0; _actions != nullptr && _actions[i] != nullptr; i++ ) {
    if (i != 0) {
      ImGui::SameLine();
    }
    
    if (ImGui::Button(_actions[i])) {
      action = i + 1;
    }
  }
  
  if ((_filterHeaderLabel != nullptr && ImGui::CollapsingHeader(_filterHeaderLabel)) || _showFilters) {
    ImGui::PushStyleColor(ImGuiCol_Button, _debugButtonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, _debugButtonHoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, _debugTextColor);
    bool ok = ImGui::Button(_debugLabel);
    ImGui::PopStyleColor(3);

    if (ok) {
      _level = Level::kDebug;
      scrollToBottom();
    }
    
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, _infoButtonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, _infoButtonHoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, _infoTextColor);
    ok = ImGui::Button(_infoLabel);
    ImGui::PopStyleColor(3);

    if (ok) {
      _level = Level::kInfo;
      scrollToBottom();
    }
    
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, _warningButtonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, _warningButtonHoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, _warningTextColor);
    ok = ImGui::Button(_warningLabel);
    ImGui::PopStyleColor(3);

    if (ok) {
      _level = Level::kWarning;
      scrollToBottom();
    }
    
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, _errorButtonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, _errorButtonHoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, _errorTextColor);
    ok = ImGui::Button(_errorLabel);
    ImGui::PopStyleColor(3);

    if (ok) {
      _level = Level::kError;
      scrollToBottom();
    }
    
    ImGui::SameLine();
    ImGui::Checkbox(_cumulativeLabel, &_cumulative);
    _filter.Draw(_filterLabel);
  }
  
  Crt::draw(reinterpret_cast<void*>(this), [](Info const& header,
                                              char const* const line,
                                              void* const ud) -> bool {

    auto const self = reinterpret_cast<Log*>(ud);
    unsigned const level = static_cast<unsigned>(self->_level);
    
    bool show = (self->_cumulative && header.metaData >= level) || header.metaData == level;
    show = show && self->_filter.PassFilter(line);

    return show;
  });

  return action;
}

void ImGuiAl::Log::setColor(Level const level, ImU32 const color) {
  ImU32 const button_color = changeValue(color, -0.2f);
  ImU32 const hovered_color = changeValue(color, -0.1f);

  switch (level) {
    case Level::kDebug:
      _debugTextColor = color;
      _debugButtonColor = button_color;
      _debugButtonHoveredColor = hovered_color;
      break;

    case Level::kInfo:
      _infoTextColor = color;
      _infoButtonColor = button_color;
      _infoButtonHoveredColor = hovered_color;
      break;

    case Level::kWarning:
      _warningTextColor = color;
      _warningButtonColor = button_color;
      _warningButtonHoveredColor = hovered_color;
      break;

    case Level::kError:
      _errorTextColor = color;
      _errorButtonColor = button_color;
      _errorButtonHoveredColor = hovered_color;
      break;
  }
}

void ImGuiAl::Log::setLabel(Level const level, char const* const label) {
  switch (level) {
    case Level::kDebug:
      _debugLabel = label;
      break;
    
    case Level::kInfo:
      _infoLabel = label;
      break;
    
    case Level::kWarning:
      _warningLabel = label;
      break;
    
    case Level::kError:
      _errorLabel = label;
      break;
  }
}

void ImGuiAl::Log::setCumulativeLabel(char const* const label) {
  _cumulativeLabel = label;
}

void ImGuiAl::Log::setFilterLabel(char const* const label) {
  _filterLabel = label;
}

void ImGuiAl::Log::setFilterHeaderLabel(char const* const label) {
  _filterHeaderLabel = label;
}

void ImGuiAl::Log::setActions(char const* actions[]) {
  _actions = actions;
}

ImGuiAl::Terminal::Terminal(void* const buffer, size_t const buffer_size, void* const cmd_buf, size_t const cmd_size)
  : Crt(buffer, buffer_size)
  , _commandBuffer(static_cast<char*>(cmd_buf))
  , _cmdBufferSize(cmd_size) {}

void ImGuiAl::Terminal::printf(char const* const format, ...) {
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}

void ImGuiAl::Terminal::draw() {
  Crt::draw();
  ImGui::Separator();

  ImGuiInputTextFlags const flags = ImGuiInputTextFlags_EnterReturnsTrue;

  bool reclaim_focus = false;

  if (ImGui::InputText("Command", _commandBuffer, _cmdBufferSize, flags, nullptr, static_cast<void*>(this))) {
    char* begin = _commandBuffer;

    while (*begin != 0 && isspace(*begin)) {
      begin++;
    }

    if (*begin != 0) {
      char* end = begin + strlen(begin) - 1;

      while (isspace(*end)) {
        end--;
      }

      end[1] = 0;
      execute(begin);
    }

    reclaim_focus = true;
  }

  ImGui::SetItemDefaultFocus();

  if (reclaim_focus) {
    ImGui::SetKeyboardFocusHere(-1);
  }
}
