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

#include "Core.h"
#include "Components.h"

#include <string>
#include <vector>
#include <stdarg.h>

namespace libretro
{
  /**
   * CoreManager contains a Core instance, and provides high level functionality.
   */
  class CoreManager {
  public:
    bool init(LoggerComponent* const logger,
              ConfigComponent* const config,
              VideoComponent* const video,
              AudioComponent* const audio,
              InputComponent* const input,
              LoaderComponent* const loader);

    bool loadCore(std::string const& corePath);
    bool loadGame(std::string const& gamePath);

    void destroy();
    
    void step();

    unsigned getApiVersion()            const { return _core.apiVersion(); }
    unsigned getRegion()                const { return _core.getRegion(); }
    void*    getMemoryData(unsigned id) const { return _core.getMemoryData(id); }
    size_t   getMemorySize(unsigned id) const { return _core.getMemorySize(id); }

    unsigned                getPerformanceLevel()    const { return _performanceLevel; }
    enum retro_pixel_format getPixelFormat()         const { return _pixelFormat; }
    bool                    getSupportsNoGame()      const { return _supportsNoGame; }
    unsigned                getRotation()            const { return _rotation; }
    bool                    getSupportAchievements() const { return _supportAchievements; }
    
    std::vector<InputDescriptor> const&  getInputDescriptors() const { return _inputDescriptors; }
    std::vector<Variable> const&         getVariables()        const { return _variables; }
    std::vector<SubsystemInfo> const&    getSubsystemInfo()    const { return _subsystemInfo; }
    std::vector<ControllerInfo> const&   getControllerInfo()   const { return _controllerInfo; }
    SystemInfo const&                    getSystemInfo()       const { return _systemInfo; }
    SystemAVInfo const&                  getSystemAVInfo()     const { return _systemAVInfo; }
    std::vector<MemoryDescriptor> const& getMemoryMap()        const { return _memoryMap; }
    
  protected:
    enum {
      kSampleCount = 8192
    };

    // Initialization
    bool initCore();
    bool initAV();
    void reset();
    
    // Shortcuts for the logging functions
    void log(enum retro_log_level const level, char const* const format, va_list args) const;
    void debug(char const* format, ...) const;
    void info(char const* format, ...) const;
    void warn(char const* format, ...) const;
    void error(char const* format, ...) const;

    // Configuration
    std::string const& getLibretroPath() const;

    // Environment functions
    bool setRotation(unsigned const data);
    bool getOverscan(bool* const data) const;
    bool getCanDupe(bool* const data) const;
    bool setMessage(struct retro_message const* const data);
    bool shutdown();
    bool setPerformanceLevel(unsigned const data);
    bool getSystemDirectory(char const** const data) const;
    bool setPixelFormat(enum retro_pixel_format const data);
    bool setInputDescriptors(struct retro_input_descriptor const* const data);
    bool setKeyboardCallback(struct retro_keyboard_callback const* const data);
    bool setDiskControlInterface(struct retro_disk_control_callback const* const data);
    bool setHWRender(retro_hw_render_callback const* const data);
    bool getVariable(struct retro_variable* const data);
    bool setVariables(struct retro_variable const* const data);
    bool getVariableUpdate(bool* const data);
    bool setSupportNoGame(bool const data);
    bool getLibretroPath(char const** const data) const;
    bool setFrameTimeCallback(struct retro_frame_time_callback const* const data);
    bool setAudioCallback(struct retro_audio_callback const* const data);
    bool getRumbleInterface(struct retro_rumble_interface* const data) const;
    bool getInputDeviceCapabilities(uint64_t* const data) const;
    bool getSensorInterface(struct retro_sensor_interface* const data) const;
    bool getCameraInterface(struct retro_camera_callback* const data) const;
    bool getLogInterface(struct retro_log_callback* const data) const;
    bool getPerfInterface(struct retro_perf_callback* const data) const;
    bool getLocationInterface(struct retro_location_callback* const data) const;
    bool getCoreAssetsDirectory(char const** const data) const;
    bool getSaveDirectory(char const** const data) const;
    bool setSystemAVInfo(struct retro_system_av_info const* const data);
    bool setProcAddressCallback(struct retro_get_proc_address_interface const* const data);
    bool setSubsystemInfo(struct retro_subsystem_info const* const data);
    bool setControllerInfo(struct retro_controller_info const* const data);
    bool setMemoryMaps(struct retro_memory_map const* const data);
    bool setGeometry(struct retro_game_geometry const* const data);
    bool getUsername(char const** const data) const;
    bool getLanguage(unsigned* const data) const;
    bool getCurrentSoftwareFramebuffer(struct retro_framebuffer* const data) const;
    bool getHWRenderInterface(struct retro_hw_render_interface const** const data) const;
    bool setSupportAchievements(bool const data);
    bool getAudioVideoEnable(int* const data) const;
    bool getCoreOptionsVersion(unsigned* const data) const;

    // Callbacks
    bool      environmentCallback(unsigned const cmd, void* const data);
    void      videoRefreshCallback(void const* const data, unsigned const width, unsigned const height, size_t const pitch);
    size_t    audioSampleBatchCallback(int16_t const* const data, size_t const frames);
    void      audioSampleCallback(int16_t const left, int16_t const right);
    int16_t   inputStateCallback(unsigned const port, unsigned const device, unsigned const index, unsigned const id);
    void      inputPollCallback();
    uintptr_t getCurrentFramebuffer();
    void      logCallback(enum retro_log_level const level, char const* const fmt, va_list args);
    retro_proc_address_t getProcAddress(char const* const symbol);

    // Static callbacks that use s_instance to call into the core's implementation
    static bool      staticEnvironmentCallback(unsigned const cmd, void* const data);

    static void      staticVideoRefreshCallback(void const* const data,
                                                unsigned const width,
                                                unsigned const height,
                                                size_t const pitch);

    static size_t    staticAudioSampleBatchCallback(const int16_t* data, size_t frames);
    static void      staticAudioSampleCallback(int16_t left, int16_t right);
    static int16_t   staticInputStateCallback(unsigned port, unsigned device, unsigned index, unsigned id);
    static void      staticInputPollCallback();
    static uintptr_t staticGetCurrentFramebuffer();
    static void      staticLogCallback(enum retro_log_level level, const char *fmt, ...);
    static retro_proc_address_t staticGetProcAddress(const char* symbol);
    
    LoggerComponent* _logger;
    ConfigComponent* _config;
    VideoComponent*  _video;
    AudioComponent*  _audio;
    InputComponent*  _input;
    LoaderComponent* _loader;
        
    Core _core;
    bool _gameLoaded;
    
    int16_t _samples[kSampleCount];
    size_t  _samplesCount;

    std::string             _libretroPath;
    unsigned                _performanceLevel;
    enum retro_pixel_format _pixelFormat;
    bool                    _supportsNoGame;
    unsigned                _rotation;
    bool                    _supportAchievements;
    
    SystemInfo   _systemInfo;
    SystemAVInfo _systemAVInfo;
    
    std::vector<InputDescriptor>  _inputDescriptors;
    std::vector<Variable>         _variables;
    std::vector<SubsystemInfo>    _subsystemInfo;
    std::vector<MemoryDescriptor> _memoryMap;
    std::vector<ControllerInfo>   _controllerInfo;
    std::vector<unsigned>         _ports;
  };
}
