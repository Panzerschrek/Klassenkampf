#pragma once
#include "Settings.hpp"
#include "TicksCounter.hpp"
#include "WindowVulkan.hpp"


namespace KK
{

class Tonemapper final
{
public:
	Tonemapper(Settings& settings, WindowVulkan& window_vulkan);
	~Tonemapper();

	vk::Extent2D GetFramebufferSize() const;
	vk::RenderPass GetDepthPrePass() const;
	vk::RenderPass GetMainRenderPass() const;
	vk::SampleCountFlagBits GetSampleCount() const;
	vk::ImageView GetDepthImageView() const;

	void DeDepthPrePass(vk::CommandBuffer command_buffer, const std::function<void()>& draw_function);
	void DoMainPass(vk::CommandBuffer command_buffer, const std::function<void()>& draw_function);
	void EndFrame(vk::CommandBuffer command_buffer);

private:
	struct BloomBuffer
	{
		vk::UniqueImage image;
		vk::UniqueDeviceMemory image_memory;
		vk::UniqueImageView image_view;
		vk::UniqueFramebuffer framebuffer;
		vk::UniqueDescriptorSet descriptor_set;
	};

	struct Pipeline
	{
		vk::UniqueShaderModule shader_vert;
		vk::UniqueShaderModule shader_frag;
		vk::UniqueSampler sampler;
		vk::UniqueDescriptorSetLayout decriptor_set_layout;
		vk::UniquePipelineLayout pipeline_layout;
		vk::UniquePipeline pipeline;
	};

private:
	Pipeline CreateMainPipeline(WindowVulkan& window_vulkan);
	Pipeline CreateBloomPipeline();

private:
	Settings& settings_;
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;
	const vk::SampleCountFlagBits msaa_sample_count_;
	TicksCounter ticks_counter_;

	vk::Extent2D framebuffer_size_;
	vk::UniqueImage framebuffer_image_;
	vk::UniqueDeviceMemory framebuffer_image_memory_;
	vk::UniqueImageView framebuffer_image_view_;

	vk::UniqueImage framebuffer_depth_image_;
	vk::UniqueDeviceMemory framebuffer_depth_image_memory_;
	vk::UniqueImageView framebuffer_depth_image_view_;

	vk::UniqueRenderPass depth_pre_pass_;
	vk::UniqueFramebuffer depth_pre_pass_framebuffer_;
	vk::UniqueRenderPass main_pass_;
	vk::UniqueFramebuffer main_pass_framebuffer_;

	// Size for brightness calculate image, bloom images.
	vk::Extent2D aux_image_size_;

	vk::UniqueImage brightness_calculate_image_;
	vk::UniqueDeviceMemory brightness_calculate_image_memory_;
	vk::UniqueImageView brightness_calculate_image_view_;
	uint32_t brightness_calculate_image_mip_levels_;

	vk::UniqueBuffer exposure_accumulate_buffer_;
	vk::UniqueDeviceMemory exposure_accumulate_memory_;
	bool exposure_buffer_prepared_= false;

	Pipeline main_pipeline_;
	Pipeline bloom_pipeline_;

	vk::UniqueDescriptorPool descriptor_pool_;

	vk::UniqueRenderPass bloom_render_pass_;
	BloomBuffer bloom_buffers_[2];

	vk::UniqueDescriptorSet main_descriptor_set_;
};

} // namespace KK
