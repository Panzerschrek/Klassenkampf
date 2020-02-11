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

	vk::Extent2D shadowmap_size_;
	vk::UniqueImage depth_image_;
	vk::UniqueDeviceMemory depth_image_memory_;
	vk::UniqueImageView depth_image_view_;

	vk::UniqueRenderPass render_pass_;
	vk::UniqueFramebuffer framebuffer_;

	vk::UniqueShaderModule shader_vert_;
	vk::UniqueShaderModule shader_frag_;
	vk::UniqueDescriptorSetLayout decriptor_set_layout_;
	vk::UniquePipelineLayout pipeline_layout_;
	vk::UniquePipeline pipeline_;
};

} // namespace KK
