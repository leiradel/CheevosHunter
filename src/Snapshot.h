#pragma once

#include "Set.h"

#include <stddef.h>
#include <stdint.h>

class Snapshot
{
public:
  enum class Size
  {
    _8,
    _16,
    _24,
    _32
  };

  enum class Format
  {
    UIntLittleEndian,
    UIntBigEndian,
    BCDLittleEndian,
    BCDBigEndian
  };

  enum class Operator
  {
    LessThan,
    LessEqual,
    GreaterThan,
    GreaterEqual,
    Equal,
    NotEqual
  };

  Snapshot(uint32_t const address, const void* const data, size_t const size);

  uint32_t address() const { return _address; }
  size_t size() const { return _size; }

  Set filter(Size const bits, Format const format, Operator const op, uint32_t const value) const;
  Set filter(Size const bits, Format const format, Operator const op, Snapshot const& other) const;

protected:
  uint32_t _address;
  void* _data;
  size_t _size;
};
