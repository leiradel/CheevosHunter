#pragma once

#include "libretro/Components.h"
#include "imguiext/imguial_term.h"
#include "speex/speex_resampler.h"

#include "json.hpp"

#include <stdarg.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

#include <SDL.h>
#include <SDL_opengl.h>

using json = nlohmann::json;

class Logger: public libretro::LoggerComponent
{
public:
  Logger() {}

  bool init();
  void destroy();
  void reset();
  void draw();

  virtual void vprintf(enum retro_log_level level, const char* fmt, va_list args) override;

protected:
  static char const* const s_actions[];

  ImGuiAl::BufferedLog<65536> _logger;
};

class Config: public libretro::ConfigComponent
{
public:
  bool init(libretro::LoggerComponent* logger, json* json);
  void destroy();
  void reset();
  void draw();

  virtual std::string const& getCoreAssetsDirectory() override;
  virtual std::string const& getSaveDirectory() override;
  virtual std::string const& getSystemPath() override;

  virtual void setVariables(std::vector<libretro::Variable> const& variables) override;
  virtual bool varUpdated() override;
  virtual std::string const& getVariable(std::string const& variable) override;

protected:
  struct Variable
  {
    std::string key;
    std::string name;
    std::string value;
    int         selected;
    std::vector<std::string> options;
  };

  libretro::LoggerComponent* _logger;
  bool        _opened;
  std::string _systemPath;
  std::string _coreAssetsPath;
  std::string _savePath;

  json* _json;

  std::map<std::string, Variable> _variables;
  bool _updated;
};

class Loader: public libretro::LoaderComponent
{
public:
  bool init(libretro::LoggerComponent* logger);
  void destroy();
  void reset();

  virtual void* load(size_t* size, std::string const& path) override;
  virtual void  free(void* data) override;

protected:
  libretro::LoggerComponent* _logger;
};
