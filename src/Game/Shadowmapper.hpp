#pragma once
#include "WindowVulkan.hpp"


namespace KK
{

class Shadowmapper final
{
public:
	explicit Shadowmapper(WindowVulkan& window_vulkan);
	~Shadowmapper();

private:
	const vk::Device vk_device_;

	const uint32_t cubemap_size_;
	const uint32_t cubemap_count_;

	vk::UniqueImage depth_cubemap_array_image_;
	vk::UniqueDeviceMemory depth_cubemap_array_image_memory_;
	vk::UniqueImageView depth_cubemap_array_image_view_;
};

} // namespace KK
