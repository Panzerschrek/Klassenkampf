#include "ShadowmapAllocator.hpp"
#include <algorithm>


namespace KK
{

namespace
{

struct LightExtra
{
	ShadowmapLight light;
	float detail_level= 0.0f;
	uint32_t detail_level_int= ~0u;
};

} // namespace

bool operator==(const ShadowmapLight& l, const ShadowmapLight& r)
{
	return l.pos == r.pos && l.radius == r.radius;
}

size_t ShadowmapLightHasher::operator()(const ShadowmapLight& light) const
{
	return
		reinterpret_cast<const uint32_t&>(light.pos.x) ^
		reinterpret_cast<const uint32_t&>(light.pos.y) ^
		reinterpret_cast<const uint32_t&>(light.pos.z) ^
		reinterpret_cast<const uint32_t&>(light.radius);
}

ShadowmapAllocator::ShadowmapAllocator(ShadowmapSize shadowmap_size)
	: shadowmap_size_(std::move(shadowmap_size))
{
	free_layers_.resize(shadowmap_size_.size());
	for(size_t layer_index= 0; layer_index < free_layers_.size(); ++layer_index)
	{
		free_layers_[layer_index].resize(shadowmap_size_[layer_index].count);
		for(size_t i= 0u; i < free_layers_[layer_index].size(); ++i)
			free_layers_[layer_index][i]= uint32_t(i);
	}
}

ShadowmapLayerIndex ShadowmapAllocator::GetLightShadowmapLayer(const ShadowmapLight& light) const
{
	const auto it= lights_set_.find(light);
	if(it == lights_set_.end())
		return c_invalid_shadowmap_layer_index;
	return it->second.shadowmap_layer;
}

ShadowmapAllocator::LightsForShadowUpdate ShadowmapAllocator::UpdateLights2(
	const std::vector<ShadowmapLight>& lights,
	const m_Vec3& cam_pos)
{
	++frame_number_;

	std::vector<LightExtra> lights_extra;
	lights_extra.reserve(lights.size());

	// Copy lights to new container.
	for(const ShadowmapLight& in_light : lights)
	{
		LightExtra out_light;
		out_light.light= in_light;
		lights_extra.push_back(std::move(out_light));
	}

	// Calculate detail level.
	for(LightExtra& light : lights_extra)
	{
		const float dist= (light.light.pos - cam_pos).GetLength();
		if(dist <= light.light.radius)
			light.detail_level= 0.0f;
		else
		{
			const float dist_k= 1.0f / 2.0f;
			light.detail_level= std::max(0.0f, std::log2((dist - light.light.radius) * dist_k));
		}
	}

	// Sort by detail level.
	std::sort(
		lights_extra.begin(),
		lights_extra.end(),
		[](const LightExtra& l, const LightExtra& r)
		{
			return l.detail_level < r.detail_level;
		});

	// Setup detail levels left array.
	std::vector<size_t> detail_levels_left(shadowmap_size_.size());
	for(size_t& s : detail_levels_left)
		s= shadowmap_size_[ &s - detail_levels_left.data() ].count;
	const uint32_t last_detail_level= uint32_t(detail_levels_left.size() - 1u);

	// Assign detail levels.
	for(LightExtra& light : lights_extra)
	{
		uint32_t detail_level_int= std::min(uint32_t(light.detail_level), last_detail_level);
		while(detail_level_int <= last_detail_level && detail_levels_left[detail_level_int] == 0u)
			++detail_level_int;

		if(detail_level_int > last_detail_level)
			continue; // No space left.

		light.detail_level_int= detail_level_int;
		--detail_levels_left[detail_level_int];
	}

	LightsForShadowUpdate lights_for_update;

	// Mark as used lights in cache.
	for(const LightExtra& light : lights_extra)
	{
		const auto it= lights_set_.find(light.light);
		if(it != lights_set_.end())
			it->second.last_used_frame_number= frame_number_;
	}

	// Assign shadowmap indices, count reculculated lights.
	for(const LightExtra& light : lights_extra)
	{
		const auto it= lights_set_.find(light.light);
		if(it == lights_set_.end())
		{
			lights_for_update.push_back(light.light);
			LightData& light_data= lights_set_[light.light];
			light_data.shadowmap_layer= AllocateLayer(light.detail_level_int);
			light_data.last_used_frame_number= frame_number_;
		}
		else
		{
			LightData& light_data= it->second;
			if(light_data.shadowmap_layer != c_invalid_shadowmap_layer_index &&
				light_data.shadowmap_layer.first != light.detail_level_int)
			{
				FreeLayer(light_data.shadowmap_layer);
				light_data.shadowmap_layer= AllocateLayer(light.detail_level_int);
				lights_for_update.push_back(light.light);
			}
		}
	}

	return lights_for_update;
}

ShadowmapLayerIndex ShadowmapAllocator::AllocateLayer(const uint32_t detail_level)
{
	if(!free_layers_[detail_level].empty())
	{
		ShadowmapLayerIndex result;
		result.first= detail_level;
		result.second= free_layers_[detail_level].back();
		free_layers_[detail_level].pop_back();
		return result;
	}

	// Search unused light with matched detail level and minimum frame number.
	uint32_t min_frame= frame_number_;
	LightData* src_light= nullptr;
	for(auto & light_pair : lights_set_)
	{
		LightData& light_data= light_pair.second;

		if(light_data.shadowmap_layer.first == detail_level &&
			light_data.last_used_frame_number < min_frame)
		{
			min_frame= light_data.last_used_frame_number;
			src_light= &light_data;
		}
	}

	if(src_light != nullptr)
	{
		const ShadowmapLayerIndex result= src_light->shadowmap_layer;
		src_light->shadowmap_layer= c_invalid_shadowmap_layer_index;
		return result;
	}

	return c_invalid_shadowmap_layer_index;
}

void ShadowmapAllocator::FreeLayer(const ShadowmapLayerIndex layer)
{
	free_layers_[layer.first].push_back(layer.second);
}

} // namespace KK
