#pragma once
#include "../MathLib/Vec.hpp"
#include "ShadowmapSize.hpp"
#include <unordered_map>
#include <vector>


namespace KK
{

struct ShadowmapLight
{
	m_Vec3 pos;
	float radius;
};

bool operator==(const ShadowmapLight& l, const ShadowmapLight& r);

struct ShadowmapLightHasher
{
	size_t operator()(const ShadowmapLight&) const;
};

class ShadowmapAllocator
{
public:
	explicit ShadowmapAllocator(ShadowmapSize shadowmap_size);

	static constexpr ShadowmapSlot c_invalid_shadowmap_slot= ShadowmapSlot(~0u, ~0u);

	using LightsForShadowUpdate= std::vector<ShadowmapLight>;
	LightsForShadowUpdate UpdateLights(const std::vector<ShadowmapLight>& lights, const m_Vec3& cam_pos);

	ShadowmapSlot GetLightShadowmapSlot(const ShadowmapLight& light) const;

private:
	struct LightData
	{
		ShadowmapSlot shadowmap_slot= c_invalid_shadowmap_slot;
		uint32_t last_used_frame_number= 0u;
	};

	struct LightHasher
	{
		size_t operator()(const ShadowmapLight&) const{ return 0u; } // TODO -implement hash
	};

	using LightsSet= std::unordered_map<ShadowmapLight, LightData, ShadowmapLightHasher>;

private:

	ShadowmapSlot AllocateSlot(uint32_t detail_level);
	void FreeSloot(ShadowmapSlot slot);

private:
	const ShadowmapSize shadowmap_size_;
	uint32_t frame_number_= 1u;
	LightsSet lights_set_;
	std::vector< std::vector<uint32_t> > free_slots_;
};

} // namespace KK
