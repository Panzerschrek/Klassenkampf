#include "ShadowmapAllocator.hpp"

namespace KK
{


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

ShadowmapAllocator::ShadowmapAllocator(const uint32_t shadowmap_layer_count)
	: shadowmap_layer_count_(shadowmap_layer_count)
{
	free_layers_.resize(shadowmap_layer_count_);
	for(uint32_t i= 0u; i < shadowmap_layer_count_; ++i)
		free_layers_[i]= shadowmap_layer_count_ - 1u - i;
}

ShadowmapAllocator::LightsForShadowUpdate ShadowmapAllocator::UpdateLights(const std::vector<ShadowmapLight>& lights)
{
	++frame_number_;

	// Update frame number of cached lights.
	uint32_t new_lights_count= 0u;
	for(const ShadowmapLight& light : lights)
	{
		const auto it= lights_set_.find(light);
		if(it != lights_set_.end())
			it->second.last_used_frame_number= frame_number_;
		else
			++new_lights_count;
	}

	// Free space for new light sources.
	{
		for(auto it= lights_set_.begin(); it != lights_set_.end() && free_layers_.size() < new_lights_count;)
		{
			if(it->second.last_used_frame_number < frame_number_)
			{
				free_layers_.push_back(it->second.shadowmap_layer);
				it= lights_set_.erase(it);
			}
			else
				++it;
		}
	}

	ShadowmapAllocator::LightsForShadowUpdate lights_for_shadow_update;

	for(const ShadowmapLight& light : lights)
	{
		const auto it= lights_set_.find(light);
		if(it == lights_set_.end())
		{
			if(free_layers_.empty())
				continue;

			lights_for_shadow_update.push_back(light);

			const uint32_t layer= free_layers_.back();
			free_layers_.pop_back();

			LightData light_data;
			light_data.shadowmap_layer= layer;
			light_data.last_used_frame_number= frame_number_;
			lights_set_.emplace(light, light_data);
		}
	}

	return lights_for_shadow_update;
}

uint32_t ShadowmapAllocator::GetLightShadowmapLayer(const ShadowmapLight& light) const
{
	const auto it= lights_set_.find(light);
	if(it == lights_set_.end())
		return c_invalid_shadowmap_layer_index;
	return it->second.shadowmap_layer;
}

} // namespace KK
