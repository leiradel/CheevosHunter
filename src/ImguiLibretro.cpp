#include "ImguiLibretro.h"

#include "fnkdat/fnkdat.h"
#include "imguiext/imguial_fonts.h"

#include <sys/stat.h>
#include <malloc.h>
#include <math.h>

bool Logger::init()
{
  static const char* actions[] =
  {
    ICON_FA_FILES_O " Copy",
    ICON_FA_FILE_O " Clear",
    NULL
  };

  _logger.setActions(actions);

  _logger.setLabel(ImGuiAl::Log::Level::kDebug, ICON_FA_BUG " Debug");
  _logger.setLabel(ImGuiAl::Log::Level::kInfo, ICON_FA_INFO " Info");
  _logger.setLabel(ImGuiAl::Log::Level::kWarning, ICON_FA_EXCLAMATION_TRIANGLE " Warn");
  _logger.setLabel(ImGuiAl::Log::Level::kError, ICON_FA_BOMB " Error");
  _logger.setCumulativeLabel(ICON_FA_SORT_AMOUNT_DESC " Cumulative");
  _logger.setFilterHeaderLabel(ICON_FA_FILTER " Filters");
  _logger.setFilterLabel(ICON_FA_SEARCH " Filter (inc,-exc)");

  return true;
}

void Logger::destroy()
{
}

void Logger::reset()
{
  _logger.clear();
}

void Logger::draw()
{
  switch (_logger.draw())
  {
  case 1:
    {
      std::string buffer;

      _logger.iterate(&buffer, [](ImGuiAl::Log::Info const& header,
                                  char const* const line,
                                  void* const ud) -> bool {

        std::string* buffer = static_cast<std::string*>(ud);

        switch (static_cast<ImGuiAl::Log::Level>(header.metaData))
        {
          case ImGuiAl::Log::Level::kDebug:   *buffer += "[DEBUG] "; break;
          case ImGuiAl::Log::Level::kInfo:    *buffer += "[INFO ] "; break;
          case ImGuiAl::Log::Level::kWarning: *buffer += "[WARN ] "; break;
          case ImGuiAl::Log::Level::kError:   *buffer += "[ERROR] "; break;
        }

        *buffer += line;
        *buffer += '\n';

        return true;
      });

      ImGui::SetClipboardText(buffer.c_str());
    }
    break;

  case 2:
    _logger.clear();
    break;
  }
}

void Logger::vprintf(enum retro_log_level level, const char* fmt, va_list args)
{
  switch (level)
  {
  case RETRO_LOG_DEBUG: _logger.debug(fmt, args); break;
  case RETRO_LOG_INFO:  _logger.info(fmt, args); break;
  case RETRO_LOG_WARN:  _logger.warning(fmt, args); break;
  default: // fallthrough
  case RETRO_LOG_ERROR: _logger.error(fmt, args); break;
  }

  _logger.scrollToBottom();
}

bool Config::init(libretro::LoggerComponent* logger, json* json)
{
  _logger = logger;
  _json = json;

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
  for (auto& pair : _variables)
  {
    Variable* var = &pair.second;

    struct Getter
    {
      static bool get(void* data, int idx, const char** out_text)
      {
        Variable const* var = (Variable*)data;

        if ((size_t)idx < var->options.size())
        {
          *out_text = var->options[idx].c_str();
          return true;
        }

        return false;
      }
    };

    int old = var->selected;
    ImGui::Combo(var->name.c_str(), &var->selected, Getter::get, (void*)var, var->options.size());

    if (old != var->selected)
    {
      (*_json)[var->key] = var->options[var->selected];
      _updated = true;
      _logger->printf(RETRO_LOG_INFO, "Variable %s changed to %s", var->name.c_str(), var->options[var->selected].c_str());
    }
  }
}

std::string const& Config::getCoreAssetsDirectory()
{
  return _coreAssetsPath;
}

std::string const& Config::getSaveDirectory()
{
  return _savePath;
}

std::string const& Config::getSystemPath()
{
  return _systemPath;
}

void Config::setVariables(std::vector<libretro::Variable> const& variables)
{
  _variables.clear();

  for (auto const& element : variables)
  {
    Variable var;
    var.key = element.key;

    size_t aux = element.value.find(';');
    var.name = element.value.substr(0, aux);
    while (isspace(element.value[++aux])) { /* Nothing. */ }

    std::string value;

    if (_json->find(var.key) != _json->end())
    {
      value = (*_json)[var.key].get<std::string>();
    }

    while (isspace(element.value[aux]))
    {
      aux++;
    }

    var.selected = 0;

    for (unsigned j = 0; aux < element.value.length(); j++)
    {
      size_t pipe = element.value.find('|', aux);
      std::string option;

      if (pipe != std::string::npos)
      {
        option = element.value.substr(aux, pipe - aux);
        aux = pipe + 1;
      }
      else
      {
        option = element.value.substr(aux);
        aux = element.value.length();
      }

      var.options.push_back(option);

      if (option == value)
      {
        var.selected = j;
        _updated = _updated || j != 0;
      }
    }

    _variables.insert(std::make_pair(var.key, var));
    (*_json)[var.key] = var.options[var.selected];
  }
}

bool Config::varUpdated()
{
  bool updated = _updated;
  _updated = false;
  return updated;
}

std::string const& Config::getVariable(std::string const& variable)
{
  static std::string const empty = "";

  auto const it = _variables.find(variable);

  if (it != _variables.end())
  {
    return it->second.options[it->second.selected];
  }

  _logger->printf(RETRO_LOG_ERROR, "Unknown variable %s", variable.c_str());
  return empty;
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

void* Loader::load(size_t* size, std::string const& path)
{
  struct stat statbuf;

  if (stat(path.c_str(), &statbuf) != 0)
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

  FILE* file = fopen(path.c_str(), "rb");

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