
#pragma once

#include <cstdint>
#include <stack>
#include <giecs/memory/accessors/deque.hpp>

namespace giecs
{
namespace memory
{
namespace accessors
{

template <std::size_t page_size, typename align_t, typename val_t>
using Stack = std::stack<val_t, Deque<page_size, align_t, std::size_t, val_t>>;

} // namespace accessors

} // namespace memory

} // namespace giecs

