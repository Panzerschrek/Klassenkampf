#pragma once
#include "WindowVulkan.hpp"


namespace KK
{

class Shadowmapper final
{
public:
	Shadowmapper(
		WindowVulkan& window_vulkan,
		size_t vertex_size,
		size_t vertex_pos_offset,
		vk::Format vertex_pos_format);
	~Shadowmapper();

	void DoRenderPass(vk::CommandBuffer command_buffer, const std::function<void()>& draw_function);

	vk::PipelineLayout GetPipelineLayout();
	vk::ImageView GetShadowmapImageView();

private:
	const vk::Device vk_device_;

	vk::Extent2D shadowmap_size_;
	vk::UniqueImage depth_image_;
	vk::UniqueDeviceMemory depth_image_memory_;
	vk::UniqueImageView depth_image_view_;

	vk::UniqueRenderPass render_pass_;
	vk::UniqueFramebuffer framebuffer_;

	vk::UniqueShaderModule shader_vert_;
	vk::UniqueDescriptorSetLayout decriptor_set_layout_;
	vk::UniquePipelineLayout pipeline_layout_;
	vk::UniquePipeline pipeline_;
};

} // namespace KK
