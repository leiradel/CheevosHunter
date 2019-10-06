#pragma once

#include <vector>
#include <algorithm>
#include <stdint.h>

class Set
{
public:
  Set() = default;
  Set(std::vector<uint32_t>&& elements);
  Set(Set&& other);

  bool contains(uint32_t element) const;

  Set union_(const Set& other);
  Set intersection(const Set& other);
  Set subtraction(const Set& other);

  std::vector<uint32_t>::const_iterator begin() const
  {
    return _elements.begin();
  }

  std::vector<uint32_t>::const_iterator end() const
  {
    return _elements.end();
  }

protected:
  std::vector<uint32_t> _elements;
};
