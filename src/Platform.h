#pragma once

#include "libretro/Core.h"

enum class Platform
{
  kNone,
  kOther,
  kNES,
  kSNES,
  kSMS,
};

Platform identifyPlatform(libretro::Core* core);
