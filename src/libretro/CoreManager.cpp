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

#include "CoreManager.h"

#include <stdlib.h>
#include <string.h>

/**
 * Unavoidable because the libretro API don't have an userdata pointer to
 * allow us pass and receive back the core instance :/
 */
static thread_local libretro::CoreManager* s_instance;

namespace {
  // Helper class to set and unset the s_instance thread local.
  class InstanceSetter {
  public:
    inline InstanceSetter(libretro::CoreManager* instance) {
      _previous = s_instance;
      s_instance = instance;
    }
    
    inline ~InstanceSetter() {
      s_instance = _previous;
    }

  protected:
    libretro::CoreManager* _previous;
  };
}

// Helper function for the memory map interface
static bool preprocessMemoryDescriptors(std::vector<libretro::MemoryDescriptor>* memoryMap);

bool libretro::CoreManager::init(LoggerComponent* const logger,
                                 ConfigComponent* const config,
                                 VideoComponent* const video,
                                 AudioComponent* const audio,
                                 InputComponent* const input,
                                 LoaderComponent* const loader) {

  InstanceSetter instance_setter(this);

  _logger = logger;
  _config = config;
  _video  = video;
  _audio  = audio;
  _input  = input;
  _loader = loader;

  reset();
  return true;
}

bool libretro::CoreManager::loadCore(std::string const& corePath) {
  InstanceSetter instance_setter(this);

  info("Opening core \"%s\"", corePath.c_str());
  _libretroPath = corePath;

  std::string message;
  
  if (!_core.load(corePath, &message)) {
    error("Error opening core \"%s\": %s", corePath, message.c_str());
    return false;
  }

  info("CoreManager \"%s\" loaded", corePath.c_str());
  return initCore();
}

bool libretro::CoreManager::loadGame(std::string const& gamePath) {
  InstanceSetter instance_setter(this);

  if (!gamePath.empty()) {
    if (_supportsNoGame) {
      error("CoreManager doesn't take a content to run");
      goto error;
    }
    
    info("Opening content \"%s\"", gamePath.c_str());

    size_t size;
    void* data = _loader->load(&size, gamePath);
  
    if (data == NULL) {
      goto error;
    }

    retro_game_info game;
    game.path = gamePath.c_str();
    game.data = data;
    game.size = size;
    game.meta = NULL;

    if (!_core.loadGame(&game)) {
      error("Error loading content");
      _loader->free(data);
      goto error;
    }

    info("Content \"%s\" loaded", gamePath.c_str());
    _loader->free(data);
  }
  else {
    if (!_supportsNoGame) {
      error("CoreManager needs content to run");
      goto error;
    }

    info("Starting core with no content");

    if (!_core.loadGame(NULL)) {
      error("Error starting core with no content");
      goto error;
    }
  }

  if (!initAV()) {
    _core.unloadGame();
    goto error;
  }

  {
    size_t const count = getControllerInfo().size();

    for (unsigned i = 0; i < count; i++) {
      _core.setControllerPortDevice(i, RETRO_DEVICE_NONE);
    }
  }
  
  _gameLoaded = true;
  return true;
  
error:
  _core.deinit();
  _core.destroy();
  reset();
  return false;
}

void libretro::CoreManager::destroy() {
  InstanceSetter instance_setter(this);

  if (_gameLoaded) {
    _core.unloadGame();
  }

  _core.deinit();
  _core.destroy();
  reset();
}

void libretro::CoreManager::step() {
  InstanceSetter instance_setter(this);

  if (_input->ctrlUpdated()) {
    size_t const count = getControllerInfo().size();

    for (unsigned i = 0; i < count; i++) {
      unsigned device = _input->getController(i);

      if (device != _ports[i]) {
        _ports[i] = device;
        _core.setControllerPortDevice(i, device);
      }
    }
  }

  _samplesCount = 0;

  do {
    _core.run();
  }
  while (_samplesCount == 0);
  
  _audio->mix(_samples, _samplesCount / 2);
}

bool libretro::CoreManager::initCore() {
  struct retro_system_info systemInfo;
  _core.getSystemInfo(&systemInfo);

  _systemInfo.libraryName     = systemInfo.library_name;
  _systemInfo.libraryVersion  = systemInfo.library_version;
  _systemInfo.validExtensions = systemInfo.valid_extensions != nullptr ? systemInfo.valid_extensions : "";
  _systemInfo.needFullpath    = systemInfo.need_fullpath;
  _systemInfo.blockExtract    = systemInfo.block_extract;

  debug("retro_system_info");
  debug("  library_name:     %s", _systemInfo.libraryName.c_str());
  debug("  library_version:  %s", _systemInfo.libraryVersion.c_str());
  debug("  valid_extensions: %s", _systemInfo.validExtensions.c_str());
  debug("  need_fullpath:    %s", _systemInfo.needFullpath ? "true" : "false");
  debug("  block_extract:    %s", _systemInfo.blockExtract ? "true" : "false");

  _core.setEnvironment(staticEnvironmentCallback);
  _core.init();
  return true;
}

bool libretro::CoreManager::initAV() {
  _core.setVideoRefresh(staticVideoRefreshCallback);
  _core.setAudioSampleBatch(staticAudioSampleBatchCallback);
  _core.setAudioSample(staticAudioSampleCallback);
  _core.setInputState(staticInputStateCallback);
  _core.setInputPoll(staticInputPollCallback);
  
  struct retro_system_av_info systemAVInfo;
  _core.getSystemAVInfo(&systemAVInfo);

  _systemAVInfo.geometry.baseWidth = systemAVInfo.geometry.base_width;
  _systemAVInfo.geometry.baseHeight = systemAVInfo.geometry.base_height;
  _systemAVInfo.geometry.maxWidth = systemAVInfo.geometry.max_width;
  _systemAVInfo.geometry.maxHeight = systemAVInfo.geometry.max_height;
  _systemAVInfo.geometry.aspectRatio = systemAVInfo.geometry.aspect_ratio;
  _systemAVInfo.timing.fps = systemAVInfo.timing.fps;
  _systemAVInfo.timing.sampleRate = systemAVInfo.timing.sample_rate;

  debug("retro_system_av_info");
  debug("  base_width   = %u", _systemAVInfo.geometry.baseWidth);
  debug("  base_height  = %u", _systemAVInfo.geometry.baseHeight);
  debug("  max_width    = %u", _systemAVInfo.geometry.maxWidth);
  debug("  max_height   = %u", _systemAVInfo.geometry.maxHeight);
  debug("  aspect_ratio = %f", _systemAVInfo.geometry.aspectRatio);
  debug("  fps          = %f", _systemAVInfo.timing.fps);
  debug("  sample_rate  = %f", _systemAVInfo.timing.sampleRate);

  if (_systemAVInfo.geometry.aspectRatio <= 0.0f) {
    _systemAVInfo.geometry.aspectRatio = (float)_systemAVInfo.geometry.baseWidth / (float)_systemAVInfo.geometry.baseHeight;
  }

  bool const videoOk = _video->setGeometry(_systemAVInfo.geometry.baseWidth,
                                           _systemAVInfo.geometry.baseHeight,
                                           _systemAVInfo.geometry.aspectRatio,
                                           _pixelFormat);

  if (!videoOk) {
    goto error;
  }
  
  if (!_audio->setRate(_systemAVInfo.timing.sampleRate)) {
    goto error;
  }
  
  return true;
  
error:
  _core.unloadGame();
  _core.deinit();
  _core.destroy();
  return false;
}

void libretro::CoreManager::reset() {
  _gameLoaded = false;
  _samplesCount = 0;
  _libretroPath.clear();
  _performanceLevel = 0;
  _pixelFormat = RETRO_PIXEL_FORMAT_UNKNOWN;
  _supportsNoGame = false;
  _rotation = 0;
  _supportAchievements = false;
  _inputDescriptors.clear();
  _variables.clear();
  memset(&_systemAVInfo, 0, sizeof(_systemAVInfo));
  _subsystemInfo.clear();
  _controllerInfo.clear();
  _ports.clear();
  _memoryMap.clear();

  _systemInfo.libraryName.clear();
  _systemInfo.libraryVersion.clear();
  _systemInfo.validExtensions.clear();
  _systemInfo.needFullpath = false;
  _systemInfo.blockExtract = false;
}

void libretro::CoreManager::log(enum retro_log_level const level, char const* const format, va_list args) const {
  _logger->vprintf(level, format, args);
}

void libretro::CoreManager::debug(char const* const format, ...) const {
  va_list args;
  va_start(args, format);
  log(RETRO_LOG_DEBUG, format, args);
  va_end(args);
}

void libretro::CoreManager::info(char const* const format, ...) const {
  va_list args;
  va_start(args, format);
  log(RETRO_LOG_INFO, format, args);
  va_end(args);
}

void libretro::CoreManager::warn(char const* const format, ...) const {
  va_list args;
  va_start(args, format);
  log(RETRO_LOG_WARN, format, args);
  va_end(args);
}

void libretro::CoreManager::error(char const* const format, ...) const {
  va_list args;
  va_start(args, format);
  log(RETRO_LOG_ERROR, format, args);
  va_end(args);
}

std::string const& libretro::CoreManager::getLibretroPath() const {
  return _libretroPath;
}

bool libretro::CoreManager::setRotation(unsigned const data) {
  _rotation = data;
  return true;
}

bool libretro::CoreManager::getOverscan(bool* const data) const {
  *data = false;
  return true;
}

bool libretro::CoreManager::getCanDupe(bool* const data) const {
  *data = true;
  return true;
}

bool libretro::CoreManager::setMessage(struct retro_message const* const data) {
  _video->showMessage(data->msg, data->frames);
  return true;
}

bool libretro::CoreManager::shutdown() {
  error("%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::CoreManager::setPerformanceLevel(unsigned const data) {
  _performanceLevel = data;
  return true;
}

bool libretro::CoreManager::getSystemDirectory(char const** const data) const {
  *data = _config->getSystemPath().c_str();
  return true;
}

bool libretro::CoreManager::setPixelFormat(enum retro_pixel_format const data) {
  switch (data) {
  case RETRO_PIXEL_FORMAT_0RGB1555:
    warn("Pixel format 0RGB1555 is deprecated");
    // fallthrough
  case RETRO_PIXEL_FORMAT_XRGB8888:
  case RETRO_PIXEL_FORMAT_RGB565:
     _pixelFormat = data;
    return true;
  
  case RETRO_PIXEL_FORMAT_UNKNOWN:
  default:
    error("Unsupported pixel format %u", data);
    return false;
  }
}

bool libretro::CoreManager::setInputDescriptors(struct retro_input_descriptor const* const data) {
  auto ptr = data;
  for (; ptr->description != nullptr; ptr++) { /* Just count. */ }

  _inputDescriptors.clear();
  _inputDescriptors.resize(ptr - data);

  ptr = data;

  for (auto& element : _inputDescriptors) {
    element.port        = ptr->port;
    element.device      = ptr->device;
    element.index       = ptr->index;
    element.id          = ptr->id;
    element.description = ptr->description;
    ptr++;
  }

  debug("retro_input_descriptor");
  debug("  port device index id description");

  for (auto const& element : _inputDescriptors) {
    debug("  %4u %6u %5u %2u %s", element.port, element.device, element.index, element.id, element.description.c_str());
  }

  _input->setInputDescriptors(_inputDescriptors);
  return true;
}

bool libretro::CoreManager::setKeyboardCallback(struct retro_keyboard_callback const* const data) {
  (void)data;
  error("%s isn't implemented", __FUNCTION__);
  return true;
}

bool libretro::CoreManager::setDiskControlInterface(struct retro_disk_control_callback const* const data) {
  (void)data;
  error("%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::CoreManager::setHWRender(struct retro_hw_render_callback const* const data) {
  (void)data;
  error("%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::CoreManager::getVariable(struct retro_variable* const data) {
  std::string const& value = _config->getVariable(data->key);
  data->value = value.c_str();
  return value.length() != 0;
}

bool libretro::CoreManager::setVariables(struct retro_variable const* const data) {
  auto ptr = data;
  for (; ptr->key != nullptr; ptr++) { /* Just count. */ }

  _variables.clear();
  _variables.resize(ptr - data);

  ptr = data;

  for (auto& element : _variables) {
    element.key = ptr->key;
    element.value = ptr->value;
    ptr++;
  }

  debug("retro_variable");
  
  for (auto const& element : _variables) {
    debug("  %s: %s", element.key.c_str(), element.value.c_str());
  }

  _config->setVariables(_variables);
  return true;
}

bool libretro::CoreManager::getVariableUpdate(bool* const data) {
  *data = _config->varUpdated();
  return true;
}

bool libretro::CoreManager::setSupportNoGame(bool const data) {
  _supportsNoGame = data;
  return true;
}

bool libretro::CoreManager::getLibretroPath(char const** const data) const {
  *data = getLibretroPath().c_str();
  return true;
}

bool libretro::CoreManager::setFrameTimeCallback(struct retro_frame_time_callback const* const data) {
  (void)data;
  error("%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::CoreManager::setAudioCallback(struct retro_audio_callback const* const data) {
  (void)data;
  error("%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::CoreManager::getRumbleInterface(struct retro_rumble_interface* const data) const {
  (void)data;
  error("%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::CoreManager::getInputDeviceCapabilities(uint64_t* const data) const {
  (void)data;
  error("%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::CoreManager::getSensorInterface(struct retro_sensor_interface* const data) const {
  (void)data;
  error("%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::CoreManager::getCameraInterface(struct retro_camera_callback* const data) const {
  (void)data;
  error("%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::CoreManager::getLogInterface(struct retro_log_callback* const data) const {
  data->log = staticLogCallback;
  return true;
}

bool libretro::CoreManager::getPerfInterface(struct retro_perf_callback* const data) const {
  (void)data;
  error("%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::CoreManager::getLocationInterface(struct retro_location_callback* const data) const {
  (void)data;
  error("%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::CoreManager::getCoreAssetsDirectory(char const** const data) const {
  *data = _config->getCoreAssetsDirectory().c_str();
  return true;
}

bool libretro::CoreManager::getSaveDirectory(char const** const data) const {
  *data = _config->getSaveDirectory().c_str();
  return true;
}

bool libretro::CoreManager::setSystemAVInfo(struct retro_system_av_info const* const data) {
  _systemAVInfo.geometry.baseWidth = data->geometry.base_width;
  _systemAVInfo.geometry.baseHeight = data->geometry.base_height;
  _systemAVInfo.geometry.maxWidth = data->geometry.max_width;
  _systemAVInfo.geometry.maxHeight = data->geometry.max_height;
  _systemAVInfo.geometry.aspectRatio = data->geometry.aspect_ratio;
  _systemAVInfo.timing.fps = data->timing.fps;
  _systemAVInfo.timing.sampleRate = data->timing.sample_rate;

  debug("retro_system_av_info");
  debug("  base_width   = %u", _systemAVInfo.geometry.baseWidth);
  debug("  base_height  = %u", _systemAVInfo.geometry.baseHeight);
  debug("  max_width    = %u", _systemAVInfo.geometry.maxWidth);
  debug("  max_height   = %u", _systemAVInfo.geometry.maxHeight);
  debug("  aspect_ratio = %f", _systemAVInfo.geometry.aspectRatio);
  debug("  fps          = %f", _systemAVInfo.timing.fps);
  debug("  sample_rate  = %f", _systemAVInfo.timing.sampleRate);

  if (_systemAVInfo.geometry.aspectRatio <= 0.0f) {
    _systemAVInfo.geometry.aspectRatio = (float)_systemAVInfo.geometry.baseWidth / (float)_systemAVInfo.geometry.baseHeight;
  }

  return _video->setGeometry(_systemAVInfo.geometry.baseWidth,
                             _systemAVInfo.geometry.baseHeight,
                             _systemAVInfo.geometry.aspectRatio,
                             _pixelFormat);
}

bool libretro::CoreManager::setProcAddressCallback(struct retro_get_proc_address_interface const* const data) {
  (void)data;
  error("%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::CoreManager::setSubsystemInfo(struct retro_subsystem_info const* const data) {
  auto ptr = data;
  for (; ptr->desc != nullptr; ptr++) { /* Just count. */ }

  _subsystemInfo.clear();
  _subsystemInfo.resize(ptr - data);

  ptr = data;

  for (auto& element : _subsystemInfo) {
    element.desc  = ptr->desc;
    element.ident = ptr->ident;
    element.roms.clear();
    element.roms.resize(ptr->num_roms);

    auto ptr2 = ptr->roms;
    ptr++;

    for (auto& element2 : element.roms) {
      element2.desc            = ptr2->desc;
      element2.validExtensions = ptr2->valid_extensions;
      element2.needFullpath    = ptr2->need_fullpath;
      element2.blockExtract    = ptr2->block_extract;
      element2.required        = ptr2->required;
      element2.memory.clear();
      element2.memory.resize(ptr2->num_memory);

      auto ptr3 = ptr2->memory;
      ptr2++;

      for (auto& element3 : element2.memory) {
        element3.extension = ptr3->extension;
        element3.type      = ptr3->type;
      }
    }
  }

  debug("retro_subsystem_info");

  for (auto const& element : _subsystemInfo) {
    debug("  desc  = %s", element.desc);
    debug("  ident = %s", element.ident);
    debug("  id    = %u", element.id);

    unsigned index2 = 0;

    for (auto const& element2 : element.roms) {
      debug("    roms[%u].desc             = %s", index2, element2.desc);
      debug("    roms[%u].valid_extensions = %s", index2, element2.validExtensions);
      debug("    roms[%u].need_fullpath    = %d", index2, element2.needFullpath);
      debug("    roms[%u].block_extract    = %d", index2, element2.blockExtract);
      debug("    roms[%u].required         = %d", index2, element2.required);

      index2++;
      unsigned index3 = 0;

      for (auto const& element3 : element2.memory) {
        debug("      memory[%u].type      = %u", index3, element3.type);
        debug("      memory[%u].extension = %s", index3, element3.extension);

        index3++;
      }
    }
  }

  return true;
}

bool libretro::CoreManager::setControllerInfo(struct retro_controller_info const* const data) {
  auto ptr = data;
  for (; ptr->types != nullptr; ptr++) { /* Just count. */ }

  _controllerInfo.clear();
  _controllerInfo.resize(ptr - data);

  _ports.clear();
  _ports.resize(ptr - data);

  ptr = data;

  for (auto& element : _controllerInfo) {
    element.types.clear();
    element.types.resize(ptr->num_types);

    auto ptr2 = ptr->types;
    ptr++;

    for (auto& element2 : element.types) {
      element2.desc = ptr2->desc;
      element2.id   = ptr2->id;
      ptr2++;
    }
  }

  for (size_t i = 0; i < _controllerInfo.size(); i++) {
    _ports[i] = RETRO_DEVICE_NONE;
  }

  debug("retro_controller_info");
  debug("  port id   desc");

  unsigned port = 0;

  for (auto const& element : _controllerInfo) {
    for (auto const& element2 : element.types) {
      debug("  %4u %04x %s", port, element2.id, element2.desc.c_str());
    }

    port++;
  }

  _input->setControllerInfo(_controllerInfo);
  return true;
}

bool libretro::CoreManager::setMemoryMaps(struct retro_memory_map const* const data) {
  _memoryMap.clear();
  _memoryMap.resize(data->num_descriptors);

  auto ptr = data->descriptors;

  for (auto& element : _memoryMap) {
    element.flags      = ptr->flags;
    element.ptr        = ptr->ptr;
    element.offset     = ptr->offset;
    element.start      = ptr->start;
    element.select     = ptr->select;
    element.disconnect = ptr->disconnect;
    element.len        = ptr->len;
    element.addrspace  = ptr->addrspace != nullptr ? ptr->addrspace : "";
    ptr++;
  }

  preprocessMemoryDescriptors(&_memoryMap);

  debug("retro_memory_map");
  debug("  flags  ptr              offset   start    select   disconn  len      addrspace");

  for (auto const& element : _memoryMap) {
    char flags[7];

    flags[0] = 'M';

    if ((element.flags & RETRO_MEMDESC_MINSIZE_8) == RETRO_MEMDESC_MINSIZE_8) {
      flags[1] = '8';
    }
    else if ((element.flags & RETRO_MEMDESC_MINSIZE_4) == RETRO_MEMDESC_MINSIZE_4) {
      flags[1] = '4';
    }
    else if ((element.flags & RETRO_MEMDESC_MINSIZE_2) == RETRO_MEMDESC_MINSIZE_2) {
      flags[1] = '2';
    }
    else {
      flags[1] = '1';
    }

    flags[2] = 'A';

    if ((element.flags & RETRO_MEMDESC_ALIGN_8) == RETRO_MEMDESC_ALIGN_8) {
      flags[3] = '8';
    }
    else if ((element.flags & RETRO_MEMDESC_ALIGN_4) == RETRO_MEMDESC_ALIGN_4) {
      flags[3] = '4';
    }
    else if ((element.flags & RETRO_MEMDESC_ALIGN_2) == RETRO_MEMDESC_ALIGN_2) {
      flags[3] = '2';
    }
    else {
      flags[3] = '1';
    }

    flags[4] = (element.flags & RETRO_MEMDESC_BIGENDIAN) ? 'B' : 'b';
    flags[5] = (element.flags & RETRO_MEMDESC_CONST) ? 'C' : 'c';
    flags[6] = 0;

    debug("  %s %p %08X %08X %08X %08X %08X %s", 
          flags,
          element.ptr,
          element.offset,
          element.start,
          element.select,
          element.disconnect,
          element.len,
          element.addrspace.c_str());
  }

  return true;
}

bool libretro::CoreManager::setGeometry(struct retro_game_geometry const* const data) {
  _systemAVInfo.geometry.baseWidth = data->base_width;
  _systemAVInfo.geometry.baseHeight = data->base_height;
  _systemAVInfo.geometry.aspectRatio = data->aspect_ratio;

  debug("retro_game_geometry");
  debug("  base_width   = %u", _systemAVInfo.geometry.baseWidth);
  debug("  base_height  = %u", _systemAVInfo.geometry.baseHeight);
  debug("  aspect_ratio = %f", _systemAVInfo.geometry.aspectRatio);

  if (_systemAVInfo.geometry.aspectRatio <= 0.0f) {
    _systemAVInfo.geometry.aspectRatio = (float)_systemAVInfo.geometry.baseWidth / _systemAVInfo.geometry.baseHeight;
  }

  return _video->setGeometry(_systemAVInfo.geometry.baseWidth,
                             _systemAVInfo.geometry.baseHeight,
                             _systemAVInfo.geometry.aspectRatio,
                             _pixelFormat);
}

bool libretro::CoreManager::getUsername(char const** const data) const {
  (void)data;
  error("%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::CoreManager::getLanguage(unsigned* const data) const {
  *data = RETRO_LANGUAGE_ENGLISH;
  return true;
}

bool libretro::CoreManager::getCurrentSoftwareFramebuffer(struct retro_framebuffer* const data) const {
  (void)data;
  error("%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::CoreManager::getHWRenderInterface(struct retro_hw_render_interface const** const data) const {
  (void)data;
  error("%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::CoreManager::setSupportAchievements(bool const data) {
  _supportAchievements = data;
  return true;
}

bool libretro::CoreManager::getAudioVideoEnable(int* const data) const {
  *data = 3; // Enable audio and video.
  return true;
}

bool libretro::CoreManager::getCoreOptionsVersion(unsigned* const data) const {
  *data = 0; // Legacy
  return true;
}

bool libretro::CoreManager::environmentCallback(unsigned const cmd, void* const data) {
  switch (cmd) {
  case RETRO_ENVIRONMENT_SET_ROTATION:
    return setRotation(*(unsigned const*)data);

  case RETRO_ENVIRONMENT_GET_OVERSCAN:
    return getOverscan((bool*)data);

  case RETRO_ENVIRONMENT_GET_CAN_DUPE:
    return getCanDupe((bool*)data);

  case RETRO_ENVIRONMENT_SET_MESSAGE:
    return setMessage((struct retro_message const*)data);

  case RETRO_ENVIRONMENT_SHUTDOWN:
    return shutdown();

  case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    return setPerformanceLevel(*(unsigned*)data);

  case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    return getSystemDirectory((char const**)data);

  case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
    return setPixelFormat(*(enum retro_pixel_format*)data);

  case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    return setInputDescriptors((struct retro_input_descriptor const*)data);

  case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
    return setKeyboardCallback((struct retro_keyboard_callback const*)data);

  case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
    return setDiskControlInterface((struct retro_disk_control_callback const*)data);

  case RETRO_ENVIRONMENT_SET_HW_RENDER:
    return setHWRender((struct retro_hw_render_callback*)data);

  case RETRO_ENVIRONMENT_GET_VARIABLE:
    return getVariable((struct retro_variable*)data);

  case RETRO_ENVIRONMENT_SET_VARIABLES:
    return setVariables((struct retro_variable const*)data);

  case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
    return getVariableUpdate((bool*)data);

  case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
    return setSupportNoGame(*(bool*)data);

  case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH:
    return getLibretroPath((char const**)data);

  case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK:
    return setFrameTimeCallback((struct retro_frame_time_callback const*)data);

  case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK:
    return setAudioCallback((struct retro_audio_callback const*)data);

  case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
    return getRumbleInterface((struct retro_rumble_interface*)data);

  case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES:
    return getInputDeviceCapabilities((uint64_t*)data);

  case RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE:
    return getSensorInterface((struct retro_sensor_interface*)data);

  case RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE:
    return getCameraInterface((struct retro_camera_callback*)data);

  case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
    return getLogInterface((struct retro_log_callback*)data);

  case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
    return getPerfInterface((struct retro_perf_callback*)data);

  case RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE:
    return getLocationInterface((struct retro_location_callback*)data);

  case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
    return getCoreAssetsDirectory((char const**)data);

  case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
    return getSaveDirectory((char const**)data);

  case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
    return setSystemAVInfo((struct retro_system_av_info const*)data);

  case RETRO_ENVIRONMENT_SET_PROC_ADDRESS_CALLBACK:
    return setProcAddressCallback((struct retro_get_proc_address_interface const*)data);

  case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
    return setSubsystemInfo((struct retro_subsystem_info const*)data);

  case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    return setControllerInfo((struct retro_controller_info const*)data);

  case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
    return setMemoryMaps((struct retro_memory_map const*)data);

  case RETRO_ENVIRONMENT_SET_GEOMETRY:
    return setGeometry((struct retro_game_geometry const*)data);

  case RETRO_ENVIRONMENT_GET_USERNAME:
    return getUsername((char const**)data);

  case RETRO_ENVIRONMENT_GET_LANGUAGE:
    return getLanguage((unsigned*)data);

  case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER:
    return getCurrentSoftwareFramebuffer((struct retro_framebuffer*)data);

  case RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE:
    return getHWRenderInterface((struct retro_hw_render_interface const**)data);

  case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
    return setSupportAchievements(*(bool*)data);
  
  case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
    return getAudioVideoEnable((int*)data);

  case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
    return getCoreOptionsVersion((unsigned*)data);

  default:
    error("Invalid env call: %u", cmd);
    return false;
  }
}

void libretro::CoreManager::videoRefreshCallback(void const* data,
                                                 unsigned const width,
                                                 unsigned const height,
                                                 size_t const pitch) {
  _video->refresh(data, width, height, pitch);
}

size_t libretro::CoreManager::audioSampleBatchCallback(int16_t const* const data, size_t const frames) {
  if (_samplesCount < kSampleCount - frames * 2 + 1) {
    memcpy(_samples + _samplesCount, data, frames * 2 * sizeof(int16_t));
    _samplesCount += frames * 2;
  }

  return frames;
}

void libretro::CoreManager::audioSampleCallback(int16_t const left, int16_t const right) {
  if (_samplesCount < kSampleCount - 1) {
    _samples[_samplesCount++] = left;
    _samples[_samplesCount++] = right;
  }
}

int16_t libretro::CoreManager::inputStateCallback(unsigned const port,
                                                  unsigned const device,
                                                  unsigned const index,
                                                  unsigned const id) {
  return _input->read(port, device, index, id);
}

void libretro::CoreManager::inputPollCallback() {
  return _input->poll();
}

uintptr_t libretro::CoreManager::getCurrentFramebuffer() {
  return _video->getCurrentFramebuffer();
}

retro_proc_address_t libretro::CoreManager::getProcAddress(char const* const symbol) {
  (void)symbol;
  return nullptr;
}

void libretro::CoreManager::logCallback(enum retro_log_level const level, char const* format, va_list args) {
  _logger->vprintf(level, format, args);
}

bool libretro::CoreManager::staticEnvironmentCallback(unsigned const cmd, void* const data) {
  return s_instance->environmentCallback(cmd, data);
}

void libretro::CoreManager::staticVideoRefreshCallback(void const* const data,
                                                       unsigned const width,
                                                       unsigned const height,
                                                       size_t const pitch) {

  s_instance->videoRefreshCallback(data, width, height, pitch);
}

size_t libretro::CoreManager::staticAudioSampleBatchCallback(int16_t const* const data, size_t const frames) {
  return s_instance->audioSampleBatchCallback(data, frames);
}

void libretro::CoreManager::staticAudioSampleCallback(int16_t const left, int16_t const right) {
  s_instance->audioSampleCallback(left, right);
}

int16_t libretro::CoreManager::staticInputStateCallback(unsigned const port,
                                                        unsigned const device,
                                                        unsigned const index,
                                                        unsigned const id) {

  return s_instance->inputStateCallback(port, device, index, id);
}

void libretro::CoreManager::staticInputPollCallback() {
  s_instance->inputPollCallback();
}

uintptr_t libretro::CoreManager::staticGetCurrentFramebuffer() {
  return s_instance->getCurrentFramebuffer();
}

retro_proc_address_t libretro::CoreManager::staticGetProcAddress(char const* const symbol) {
  return s_instance->getProcAddress(symbol);
}

void libretro::CoreManager::staticLogCallback(enum retro_log_level const level, char const* const format, ...) {
  va_list args;
  va_start(args, format);
  s_instance->log(level, format, args);
  va_end(args);
}

static size_t addBitsDown(size_t n) {
  n |= n >>  1;
  n |= n >>  2;
  n |= n >>  4;
  n |= n >>  8;
  n |= n >> 16;

  /* double shift to avoid warnings on 32bit (it's dead code, but compilers suck) */
  if (sizeof(size_t) > 4) {
    n |= n >> 16 >> 16;
  }

  return n;
}

static size_t inflate(size_t addr, size_t mask) {
  while (mask) {
    size_t tmp = (mask - 1) & ~mask;
    /* to put in an 1 bit instead, OR in tmp+1 */
    addr = ((addr & ~tmp) << 1) | (addr & tmp);
    mask = mask & (mask - 1);
  }

  return addr;
}

static size_t reduce(size_t addr, size_t mask) {
  while (mask) {
    size_t tmp = (mask - 1) & ~mask;
    addr = (addr & tmp) | ((addr >> 1) & ~tmp);
    mask = (mask & (mask - 1)) >> 1;
  }

  return addr;
}

static size_t highestBit(size_t n) {
   n = addBitsDown(n);
   return n ^ (n >> 1);
}

static bool preprocessMemoryDescriptors(std::vector<libretro::MemoryDescriptor>* memoryMap) {
  size_t disconnect_mask;
  size_t top_addr = 1;

  for (auto& element : *memoryMap) {
    if (element.select != 0) {
      top_addr |= element.select;
    }
    else {
      top_addr |= element.start + element.len - 1;
    }
  }

  top_addr = addBitsDown(top_addr);

  for (auto& element : *memoryMap) {
    if (element.select == 0) {
      if (element.len == 0) {
        return false;
      }

      if ((element.len & (element.len - 1)) != 0) {
        return false;
      }

      element.select = top_addr & ~inflate(addBitsDown(element.len - 1), element.disconnect);
    }

    if (element.len == 0) {
      element.len = addBitsDown(reduce(top_addr & ~element.select, element.disconnect)) + 1;
    }

    if (element.start & ~element.select) {
      return false;
    }

    while (reduce(top_addr & ~element.select, element.disconnect) >> 1 > element.len - 1) {
      element.disconnect |= highestBit(top_addr & ~element.select & ~element.disconnect);
    }

    disconnect_mask = addBitsDown(element.len - 1);
    element.disconnect &= disconnect_mask;

    while ((~disconnect_mask) >> 1 & element.disconnect) {
      disconnect_mask >>= 1;
      element.disconnect &= disconnect_mask;
    }
  }

  return true;
}
