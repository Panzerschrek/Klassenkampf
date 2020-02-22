#pragma once
#include "../MathLib/Vec.hpp"
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
	explicit ShadowmapAllocator(uint32_t shadowmap_layer_count);

	static constexpr uint32_t c_invalid_shadowmap_layer_index= ~0u;

	using LightsForShadowUpdate= std::vector<ShadowmapLight>;
	LightsForShadowUpdate UpdateLights(const std::vector<ShadowmapLight>& lights);

	uint32_t GetLightShadowmapLayer(const ShadowmapLight& light) const;

private:
	struct LightData
	{
		uint32_t shadowmap_layer= c_invalid_shadowmap_layer_index;
		uint32_t last_used_frame_number= 0u;
	};

	struct LightHasher
	{
		size_t operator()(const ShadowmapLight&) const{ return 0u; } // TODO -implement hash
	};

	using LightsSet= std::unordered_map<ShadowmapLight, LightData, ShadowmapLightHasher>;

private:
	const uint32_t shadowmap_layer_count_;
	uint32_t frame_number_= 0u;
	LightsSet lights_set_;
	std::vector<uint32_t> free_layers_;
};

} // namespace KK
