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

#include "Core.h"

#include <sys/stat.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define CORE_DLSYM(prop, name) \
  do { \
    void* sym = dynlib_symbol(_handle, name); \
    if (!sym) goto error; \
    memcpy(&prop, &sym, sizeof(prop)); \
  } while (0)

bool libretro::Core::load(std::string const& path, std::string* const error) {
  _handle = dynlib_open(path.c_str());

  if (_handle == NULL) {
    if (error != nullptr) {
      *error = dynlib_error();
    }
    
    return false;
  }

  CORE_DLSYM(_init, "retro_init");
  CORE_DLSYM(_deinit, "retro_deinit");
  CORE_DLSYM(_apiVersion, "retro_api_version");
  CORE_DLSYM(_getSystemInfo, "retro_get_system_info");
  CORE_DLSYM(_getSystemAVInfo, "retro_get_system_av_info");
  CORE_DLSYM(_setEnvironment, "retro_set_environment");
  CORE_DLSYM(_setVideoRefresh, "retro_set_video_refresh");
  CORE_DLSYM(_setAudioSample, "retro_set_audio_sample");
  CORE_DLSYM(_setAudioSampleBatch, "retro_set_audio_sample_batch");
  CORE_DLSYM(_setInputPoll, "retro_set_input_poll");
  CORE_DLSYM(_setInputState, "retro_set_input_state");
  CORE_DLSYM(_setControllerPortDevice, "retro_set_controller_port_device");
  CORE_DLSYM(_reset, "retro_reset");
  CORE_DLSYM(_run, "retro_run");
  CORE_DLSYM(_serializeSize, "retro_serialize_size");
  CORE_DLSYM(_serialize, "retro_serialize");
  CORE_DLSYM(_unserialize, "retro_unserialize");
  CORE_DLSYM(_cheatReset, "retro_cheat_reset");
  CORE_DLSYM(_cheatSet, "retro_cheat_set");
  CORE_DLSYM(_loadGame, "retro_load_game");
  CORE_DLSYM(_loadGameSpecial, "retro_load_game_special");
  CORE_DLSYM(_unloadGame, "retro_unload_game");
  CORE_DLSYM(_getRegion, "retro_get_region");
  CORE_DLSYM(_getMemoryData, "retro_get_memory_data");
  CORE_DLSYM(_getMemorySize, "retro_get_memory_size");

  return true;

error:
  if (error != nullptr) {
    *error = dynlib_error();
  }
  
  dynlib_close(_handle);
  return false;
}

#undef CORE_DLSYM

void libretro::Core::destroy() {
  dynlib_close(_handle);
}
