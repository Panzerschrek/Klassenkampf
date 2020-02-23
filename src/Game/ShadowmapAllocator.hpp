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

	static constexpr ShadowmapLayerIndex c_invalid_shadowmap_layer_index= ShadowmapLayerIndex(~0u, ~0u);

	using LightsForShadowUpdate= std::vector<ShadowmapLight>;
	LightsForShadowUpdate UpdateLights2(const std::vector<ShadowmapLight>& lights, const m_Vec3& cam_pos);

	ShadowmapLayerIndex GetLightShadowmapLayer(const ShadowmapLight& light) const;

private:
	struct LightData
	{
		ShadowmapLayerIndex shadowmap_layer= c_invalid_shadowmap_layer_index;
		uint32_t last_used_frame_number= 0u;
	};

	struct LightHasher
	{
		size_t operator()(const ShadowmapLight&) const{ return 0u; } // TODO -implement hash
	};

	using LightsSet= std::unordered_map<ShadowmapLight, LightData, ShadowmapLightHasher>;

private:

	ShadowmapLayerIndex AllocateLayer(uint32_t detail_level);
	void FreeLayer(ShadowmapLayerIndex layer);

private:
	const ShadowmapSize shadowmap_size_;
	uint32_t frame_number_= 0u;
	LightsSet lights_set_;
	std::vector< std::vector<uint32_t> > free_layers_;
};

} // namespace KK
