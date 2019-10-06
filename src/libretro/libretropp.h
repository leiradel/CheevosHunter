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

#include "libretro.h"
#include <string>
#include <vector>

namespace libretro {
    struct InputDescriptor {
      unsigned port;
      unsigned device;
      unsigned index;
      unsigned id;
      std::string description;
    };

    struct Variable {
      std::string key;
      std::string value;
    };

    struct SubsystemInfo {
      struct RomInfo {
        struct MemoryInfo {
          std::string extension;
          unsigned type;
        };

        std::string desc;
        std::string validExtensions;
        bool needFullpath;
        bool blockExtract;
        bool required;

        std::vector<MemoryInfo> memory;
      };

      std::string desc;
      std::string ident;
      std::vector<RomInfo> roms;
      unsigned id;
    };

    struct ControllerDescription {
      std::string desc;
      unsigned id;
    };

    struct ControllerInfo {
      std::vector<ControllerDescription> types;
    };

    struct SystemInfo {
      std::string libraryName;
      std::string libraryVersion;
      std::string validExtensions;
      bool needFullpath;
      bool blockExtract;
    };

    struct SystemAVInfo {
      struct Geometry {
        unsigned baseWidth;
        unsigned baseHeight;
        unsigned maxWidth;
        unsigned maxHeight;
        float aspectRatio;
      };

      struct Timing {
        double fps;
        double sampleRate;
      };

      Geometry geometry;
      Timing timing;
    };

    struct MemoryDescriptor {
      uint64_t flags;
      void *ptr;
      size_t offset;
      size_t start;
      size_t select;
      size_t disconnect;
      size_t len;
      std::string addrspace;
    };
}
