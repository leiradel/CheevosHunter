#include "Set.h"

#include <algorithm>

Set::Set(std::vector<uint32_t>&& elements) : _elements(std::move(elements)) {
  std::sort(_elements.begin(), _elements.end(), [](uint32_t const& a, uint32_t const& b) -> bool {
    return a < b;
  });
}

Set::Set(Set&& other) : _elements(std::move(other._elements)) {}

bool Set::contains(uint32_t element) const
{
  return std::binary_search(_elements.begin(), _elements.end(), element);
}

Set Set::union_(const Set& other)
{
  Set result;
  result._elements.reserve(_elements.size() + other._elements.size());

  std::set_union(_elements.begin(),
                 _elements.end(),
                 other._elements.begin(),
                 other._elements.end(),
                 std::inserter(result._elements, result._elements.begin()));

  result._elements.shrink_to_fit();
  return result;
}

Set Set::intersection(const Set& other)
{
  Set result;
  result._elements.reserve(std::min(_elements.size(), other._elements.size()));

  std::set_intersection(_elements.begin(),
                        _elements.end(),
                        other._elements.begin(),
                        other._elements.end(),
                        std::inserter(result._elements, result._elements.begin()));

  result._elements.shrink_to_fit();
  return result;
}

Set Set::subtraction(const Set& other)
{
  Set result;
  result._elements.reserve(_elements.size());

  std::set_difference(_elements.begin(),
                      _elements.end(),
                      other._elements.begin(),
                      other._elements.end(),
                      std::inserter(result._elements, result._elements.begin()));

  result._elements.shrink_to_fit();
  return result;
}
