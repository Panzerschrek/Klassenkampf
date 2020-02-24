#pragma once
#include <cstdint>
#include <vector>


namespace KK
{

struct ShadowmapLevelSize
{
	uint32_t size;
	uint32_t count;
};

// Must be sorted by size in descent order.
using ShadowmapSize= std::vector<ShadowmapLevelSize>;

// First - number of cubemap array, second - number of layer in array.
using ShadowmapSlot= std::pair<uint32_t, uint32_t>;

} // namespace KK
