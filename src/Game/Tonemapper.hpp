#pragma once
#include "WindowVulkan.hpp"


namespace KK
{

class Tonemapper final
{
public:
	explicit Tonemapper(WindowVulkan& window_vulkan);
	~Tonemapper();

	vk::Extent2D GetFramebufferSize() const;
	vk::RenderPass GetRenderPass() const;
	vk::SampleCountFlagBits GetSampleCount() const;

	void DoRenderPass(vk::CommandBuffer command_buffer, const std::function<void()>& draw_function);
	void EndFrame(vk::CommandBuffer command_buffer);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;
	const vk::SampleCountFlagBits msaa_sample_count_;

	vk::Extent2D framebuffer_size_;
	vk::UniqueImage framebuffer_image_;
	vk::UniqueDeviceMemory framebuffer_image_memory_;
	vk::UniqueImageView framebuffer_image_view_;
	vk::UniqueSampler framebuffer_image_sampler_;
	uint32_t framebuffer_image_mip_levels_;

	vk::UniqueImage framebuffer_depth_image_;
	vk::UniqueDeviceMemory framebuffer_depth_image_memory_;
	vk::UniqueImageView framebuffer_depth_image_view_;

	vk::UniqueRenderPass framebuffer_render_pass_;
	vk::UniqueFramebuffer framebuffer_;

	vk::UniqueShaderModule shader_vert_;
	vk::UniqueShaderModule shader_frag_;
	vk::UniqueDescriptorSetLayout decriptor_set_layout_;
	vk::UniquePipelineLayout pipeline_layout_;
	vk::UniquePipeline pipeline_;

	vk::UniqueDescriptorPool descriptor_pool_;
	vk::UniqueDescriptorSet descriptor_set_;
};

} // namespace KK
