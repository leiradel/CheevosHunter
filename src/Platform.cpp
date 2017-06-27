#include "Platform.h"

static uint32_t djb2(const char* str)
{
  uint32_t hash = 5381;                
                                       
  while (*str != 0)                       
  {                                    
    hash = hash * 33 + (uint8_t)*str++;
  }                                    
                                       
  return hash;
}

Platform identifyPlatform(libretro::Core* core)
{
  const struct retro_system_info* info = core->getSystemInfo();

  switch (djb2(info->library_name))
  {
  case 0x7c94ae0dU: // bnes
  case 0xc3eefa9bU: // emux (nes)
  case 0xb00bd8c2U: // FCEUmm
  case 0xeb2f41e8U: // Nestopia
  case 0x670e8ee8U: // QuickNES - this is the only core that implements one of the required interfaces
    return Platform::kNES;

  case 0x0f2d5280U: // bsnes
  case 0x0f1b3a00U: // bSNES
  case 0x48965794U: // bsnes-mercury
  case 0xd17b0bafU: // Snes9x
  case 0x39affeb6U: // Snes9x 2005
  case 0x39affed2U: // Snes9x 2010
  case 0x28f2d47eU: // Mednafen bSNES
    return Platform::kSNES;
  
  case 0x2ce692d6U: // Genesis Plus GX
  case 0x0cc11b6aU: // PicoDrive
    return Platform::kSMS;
  }

  return Platform::kOther;
}
