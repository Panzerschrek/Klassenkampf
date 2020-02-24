#include "ShadowmapAllocator.hpp"
#include "Assert.hpp"
#include <algorithm>
#include <cstring>


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
	// Use memcmp, because floating point compare is incorrect for hashing.
	return std::memcmp(&l, &r, sizeof(ShadowmapLight)) == 0;
}

size_t ShadowmapLightHasher::operator()(const ShadowmapLight& light) const
{
	// HACK! Calculate hash for floats as hash for ints.
	static_assert(sizeof(ShadowmapLight) == sizeof(uint32_t) * 4u, "Invalid size");
	const auto ints= reinterpret_cast<const uint32_t*>(&light);
	return ints[0] ^ ints[1] ^ ints[2] ^ ints[3];
}

ShadowmapAllocator::ShadowmapAllocator(ShadowmapSize shadowmap_size)
	: shadowmap_size_(std::move(shadowmap_size))
{
	free_slots_.resize(shadowmap_size_.size());
	for(size_t detail_level= 0; detail_level < free_slots_.size(); ++detail_level)
	{
		free_slots_[detail_level].resize(shadowmap_size_[detail_level].count);
		for(size_t i= 0u; i < free_slots_[detail_level].size(); ++i)
			free_slots_[detail_level][i]= uint32_t(i);
	}
}

ShadowmapSlot ShadowmapAllocator::GetLightShadowmapSlot(const ShadowmapLight& light) const
{
	const auto it= lights_set_.find(light);
	if(it == lights_set_.end())
		return c_invalid_shadowmap_slot;
	return it->second.shadowmap_slot;
}

ShadowmapAllocator::LightsForShadowUpdate ShadowmapAllocator::UpdateLights(
	const std::vector<ShadowmapLight>& lights,
	const m_Vec3& cam_pos)
{
	++frame_number_;

	// Copy lights to new container.
	std::vector<LightExtra> lights_extra;
	lights_extra.reserve(lights.size());
	for(const ShadowmapLight& in_light : lights)
	{
		LightExtra out_light;
		out_light.light= in_light;
		lights_extra.push_back(std::move(out_light));
	}

	// Calculate detail level.
	for(LightExtra& light : lights_extra)
	{
		// TODO - formulas here are totaly wrong, do real math to calculate detalization.
		const float dist= (light.light.pos - cam_pos).GetLength();
		const float dist_k= 3.0f;
		light.detail_level= std::log2(std::max(dist_k * dist / light.light.radius, 1.0f));
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
		{
			KK_ASSERT(false); // TODO - handle this situation
			continue; // No space left.
		}

		light.detail_level_int= detail_level_int;
		--detail_levels_left[detail_level_int];
	}

	// Mark as used lights in cache, free layers of lights, that changed their detail level.
	for(const LightExtra& light : lights_extra)
	{
		const auto it= lights_set_.find(light.light);
		if(it == lights_set_.end())
			continue;

		LightData& light_data= it->second;
		light_data.last_used_frame_number= frame_number_;
		if(light_data.shadowmap_slot.first != light.detail_level_int &&
			light_data.shadowmap_slot != c_invalid_shadowmap_slot)
		{
			FreeSloot(light_data.shadowmap_slot);
			light_data.shadowmap_slot= c_invalid_shadowmap_slot;
		}
	}

	// Assign shadowmap indices, count recalculated lights.
	LightsForShadowUpdate lights_for_update;
	for(const LightExtra& light : lights_extra)
	{
		const auto it= lights_set_.find(light.light);
		if(it == lights_set_.end())
		{
			LightData& light_data= lights_set_[light.light];
			light_data.last_used_frame_number= frame_number_;
			light_data.shadowmap_slot= AllocateSlot(light.detail_level_int);
			if(light_data.shadowmap_slot != c_invalid_shadowmap_slot)
				lights_for_update.push_back(light.light);
		}
		else
		{
			LightData& light_data= it->second;
			KK_ASSERT(light_data.last_used_frame_number == frame_number_);
			if(light_data.shadowmap_slot.first != light.detail_level_int)
			{
				KK_ASSERT(light_data.shadowmap_slot == c_invalid_shadowmap_slot);
				light_data.shadowmap_slot= AllocateSlot(light.detail_level_int);
				if(light_data.shadowmap_slot != c_invalid_shadowmap_slot)
					lights_for_update.push_back(light.light);
			}
		}
	}

	return lights_for_update;
}

ShadowmapSlot ShadowmapAllocator::AllocateSlot(const uint32_t detail_level)
{
	if(!free_slots_[detail_level].empty())
	{
		ShadowmapSlot result;
		result.first= detail_level;
		result.second= free_slots_[detail_level].back();
		free_slots_[detail_level].pop_back();
		return result;
	}

	// Search unused light with matched detail level and minimum frame number.
	uint32_t min_frame= frame_number_;
	LightsSet::value_type* src_light= nullptr;
	for(auto& light_pair : lights_set_)
	{
		LightData& light_data= light_pair.second;

		if( light_data.shadowmap_slot != c_invalid_shadowmap_slot &&
			light_data.shadowmap_slot.first == detail_level &&
			light_data.last_used_frame_number < min_frame)
		{
			min_frame= light_data.last_used_frame_number;
			src_light= &light_pair;
		}
	}

	if(src_light != nullptr)
	{
		const ShadowmapSlot result= src_light->second.shadowmap_slot;
		lights_set_.erase(src_light->first);
		return result;
	}

	return c_invalid_shadowmap_slot;
}

void ShadowmapAllocator::FreeSloot(const ShadowmapSlot slot)
{
	free_slots_[slot.first].push_back(slot.second);
}

} // namespace KK
