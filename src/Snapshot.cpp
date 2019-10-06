#include "Snapshot.h"

#include <stdlib.h>
#include <string.h>

Snapshot::Snapshot(uint32_t const address, const void* const data, size_t const size) {
  _address = address;
  _size = size;

  _data = malloc(size);
  memcpy(_data, data, size);
}

template<Snapshot::Operator O>
static bool compare(uint32_t const v1, uint32_t const v2) {
  switch (O) {
  case Snapshot::Operator::LessThan:     return v1 < v2;
  case Snapshot::Operator::LessEqual:    return v1 <= v2;
  case Snapshot::Operator::GreaterThan:  return v1 > v2;
  case Snapshot::Operator::GreaterEqual: return v1 >= v2;
  case Snapshot::Operator::Equal:        return v1 == v2;
  case Snapshot::Operator::NotEqual:     return v1 != v2;
  }
}

template<Snapshot::Format F>
static uint32_t convert(uint32_t v) {
  switch (F) {
    case Snapshot::Format::UIntLittleEndian:
      return (v >> 24 & UINT32_C(0x000000ff)) |
             (v >>  8 & UINT32_C(0x0000ff00)) |
             (v <<  8 & UINT32_C(0x00ff0000)) |
             (v << 24 & UINT32_C(0xff000000));

    case Snapshot::Format::UIntBigEndian:
      return v;

    case Snapshot::Format::BCDLittleEndian:
      v = convert<Snapshot::Format::UIntLittleEndian>(v);
      // fallthrough

    case Snapshot::Format::BCDBigEndian:
      return (v >>  0 & 15) * UINT32_C(       1) +
             (v >>  4 & 15) * UINT32_C(      10) +
             (v >>  8 & 15) * UINT32_C(     100) +
             (v >> 12 & 15) * UINT32_C(    1000) +
             (v >> 16 & 15) * UINT32_C(   10000) +
             (v >> 20 & 15) * UINT32_C(  100000) +
             (v >> 24 & 15) * UINT32_C( 1000000) +
             (v >> 28 & 15) * UINT32_C(10000000);
  }
}

template<size_t S, Snapshot::Format F, Snapshot::Operator O>
static Set filter(uint32_t address, void const* const data, size_t const size, uint32_t const value) {
  if (S > size) {
    return Set();
  }

  std::vector<uint32_t> result;

  auto bytes = static_cast<const uint8_t*>(data);
  auto const end = bytes + size;

  uint32_t current = *bytes++;

  if (S >= 2) {
    current |= static_cast<uint32_t>(*bytes++) << 8;
  }

  if (S >= 3) {
    current |= static_cast<uint32_t>(*bytes++) << 16;
  }

  if (S >= 4) {
    current |= static_cast<uint32_t>(*bytes++) << 24;
  }

  for (;;) {
    if (compare<O>(convert<F>(current), value)) {
      result.emplace_back(address);
    }

    if (bytes >= end) {
      return Set(std::move(result));
    }

    current >>= 8;
    current |= static_cast<uint32_t>(*bytes++) << ((S - 1) * 8);
    address++;
  }
}

template<size_t S, Snapshot::Format F>
static Set filter(Snapshot::Operator const op,
                  uint32_t const address,
                  void const* const data,
                  size_t const size,
                  uint32_t const value) {

  switch (op) {
    default: // never happens
    case Snapshot::Operator::LessThan:
      return filter<S, F, Snapshot::Operator::LessThan>(address, data, size, value);

    case Snapshot::Operator::LessEqual:
      return filter<S, F, Snapshot::Operator::LessEqual>(address, data, size, value);

    case Snapshot::Operator::GreaterThan:
      return filter<S, F, Snapshot::Operator::GreaterThan>(address, data, size, value);

    case Snapshot::Operator::GreaterEqual:
      return filter<S, F, Snapshot::Operator::GreaterEqual>(address, data, size, value);

    case Snapshot::Operator::Equal:
      return filter<S, F, Snapshot::Operator::Equal>(address, data, size, value);

    case Snapshot::Operator::NotEqual:
      return filter<S, F, Snapshot::Operator::NotEqual>(address, data, size, value);
  }
}

template<size_t S>
static Set filter(Snapshot::Format const format,
                  Snapshot::Operator const op,
                  uint32_t const address,
                  void const* const data,
                  size_t const size,
                  uint32_t const value) {

  switch (format) {
    default: // never happens
    case Snapshot::Format::UIntLittleEndian:
      return filter<S, Snapshot::Format::UIntLittleEndian>(op, address, data, size, value);

    case Snapshot::Format::UIntBigEndian:
      return filter<S, Snapshot::Format::UIntBigEndian>(op, address, data, size, value);

    case Snapshot::Format::BCDLittleEndian:
      return filter<S, Snapshot::Format::BCDLittleEndian>(op, address, data, size, value);

    case Snapshot::Format::BCDBigEndian:
      return filter<S, Snapshot::Format::BCDBigEndian>(op, address, data, size, value);
  }
}

static Set filter(Snapshot::Size const bits,
                  Snapshot::Format const format,
                  Snapshot::Operator op,
                  uint32_t const address,
                  void const* const data,
                  size_t const size,
                  uint32_t const value) {

  switch (bits) {
    default: // never happens
    case Snapshot::Size::_8:  return filter<1>(format, op, address, data, size, value);
    case Snapshot::Size::_16: return filter<2>(format, op, address, data, size, value);
    case Snapshot::Size::_24: return filter<3>(format, op, address, data, size, value);
    case Snapshot::Size::_32: return filter<4>(format, op, address, data, size, value);
  }
}

Set Snapshot::filter(Size const bits, Format const format, Operator const op, uint32_t const value) const {
  return ::filter(bits, format, op, _address, _data, _size, value);
}

template<size_t S, Snapshot::Format F, Snapshot::Operator O>
static Set filter(uint32_t address, size_t const size, void const* const data1, void const* const data2) {
  if (S > size) {
    return Set();
  }

  std::vector<uint32_t> result;

  auto bytes1 = static_cast<const uint8_t*>(data1);
  auto bytes2 = static_cast<const uint8_t*>(data2);
  auto const end1 = bytes1 + size;

  uint32_t current1 = *bytes1++;
  uint32_t current2 = *bytes2++;

  if (S >= 2) {
    current1 |= static_cast<uint32_t>(*bytes1++) << 8;
    current2 |= static_cast<uint32_t>(*bytes2++) << 8;
  }

  if (S >= 3) {
    current1 |= static_cast<uint32_t>(*bytes1++) << 16;
    current2 |= static_cast<uint32_t>(*bytes2++) << 16;
  }

  if (S >= 4) {
    current1 |= static_cast<uint32_t>(*bytes1++) << 24;
    current2 |= static_cast<uint32_t>(*bytes2++) << 24;
  }

  for (;;) {
    if (compare<O>(convert<F>(current1), convert<F>(current2))) {
      result.emplace_back(address);
    }

    if (bytes1 >= end1) {
      return Set(std::move(result));
    }

    current1 >>= 8;
    current1 |= static_cast<uint32_t>(*bytes1++) << ((S - 1) * 8);

    current2 >>= 8;
    current2 |= static_cast<uint32_t>(*bytes2++) << ((S - 1) * 8);

    address++;
  }
}

template<size_t S, Snapshot::Format F>
static Set filter(Snapshot::Operator const op,
                  uint32_t const address,
                  size_t const size,
                  void const* const data1,
                  void const* const data2) {

  switch (op) {
    default: // never happens
    case Snapshot::Operator::LessThan:
      return filter<S, F, Snapshot::Operator::LessThan>(address, size, data1, data2);

    case Snapshot::Operator::LessEqual:
      return filter<S, F, Snapshot::Operator::LessEqual>(address, size, data1, data2);

    case Snapshot::Operator::GreaterThan:
      return filter<S, F, Snapshot::Operator::GreaterThan>(address, size, data1, data2);

    case Snapshot::Operator::GreaterEqual:
      return filter<S, F, Snapshot::Operator::GreaterEqual>(address, size, data1, data2);

    case Snapshot::Operator::Equal:
      return filter<S, F, Snapshot::Operator::Equal>(address, size, data1, data2);

    case Snapshot::Operator::NotEqual:
      return filter<S, F, Snapshot::Operator::NotEqual>(address, size, data1, data2);
  }
}

template<size_t S>
static Set filter(Snapshot::Format const format,
                  Snapshot::Operator const op,
                  uint32_t const address,
                  size_t const size,
                  void const* const data1,
                  void const* const data2) {

  switch (format) {
    default: // never happens
    case Snapshot::Format::UIntLittleEndian:
      return filter<S, Snapshot::Format::UIntLittleEndian>(op, address, size, data1, data2);

    case Snapshot::Format::UIntBigEndian:
      return filter<S, Snapshot::Format::UIntBigEndian>(op, address, size, data1, data2);

    case Snapshot::Format::BCDLittleEndian:
      return filter<S, Snapshot::Format::BCDLittleEndian>(op, address, size, data1, data2);

    case Snapshot::Format::BCDBigEndian:
      return filter<S, Snapshot::Format::BCDBigEndian>(op, address, size, data1, data2);
  }
}

static Set filter(Snapshot::Size const bits,
                  Snapshot::Format const format,
                  Snapshot::Operator op,
                  uint32_t const address1,
                  void const* const data1,
                  size_t const size1,
                  uint32_t const address2,
                  void const* const data2,
                  size_t const size2) {

  if (address1 != address2 || size1 != size2) {
    return Set();
  }

  switch (bits) {
    default: // never happens
    case Snapshot::Size::_8:  return filter<1>(format, op, address1, size1, data1, data2);
    case Snapshot::Size::_16: return filter<2>(format, op, address1, size1, data1, data2);
    case Snapshot::Size::_24: return filter<3>(format, op, address1, size1, data1, data2);
    case Snapshot::Size::_32: return filter<4>(format, op, address1, size1, data1, data2);
  }
}

Set Snapshot::filter(Size const bits, Format const format, Operator const op, Snapshot const& other) const {
  return ::filter(bits, format, op, _address, _data, _size, other._address, other._data, other._size);
}
