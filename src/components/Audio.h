#pragma once

#include "libretro/Components.h"
#include "speex/speex_resampler.h"

#include <SDL.h>

class Fifo {
public:
  bool init(size_t const size);
  void destroy();
  void reset();

  void read(void* const data, size_t const size);
  void write(void const* const data, size_t const size);

  size_t size();
  size_t occupied();
  size_t free();

protected:
  SDL_mutex* _mutex;
  uint8_t*   _buffer;
  size_t     _size;
  size_t     _avail;
  size_t     _first;
  size_t     _last;
};

class Audio: public libretro::AudioComponent {
public:
  bool init(libretro::LoggerComponent* const logger, double const sample_rate, Fifo* const fifo);
  void destroy();
  void reset();
  void draw();

  virtual bool setRate(double const rate) override;
  virtual void mix(int16_t const* const samples, size_t const frames) override;

protected:
  libretro::LoggerComponent* _logger;

  bool           _mute;
  int16_t const* _samples;
  size_t         _frames;
  float          _min;
  float          _max;

  double _sampleRate;
  double _coreRate;

  double _rateControlDelta;
  double _currentRatio;
  double _originalRatio;
  SpeexResamplerState* _resampler;

  Fifo* _fifo;
};
