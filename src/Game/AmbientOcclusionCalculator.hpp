#pragma once
#include "GPUDataUploader.hpp"
#include "Tonemapper.hpp"


namespace KK
{

class AmbientOcclusionCalculator
{
public:
	AmbientOcclusionCalculator(
		Settings& settings,
		WindowVulkan& window_vulkan,
		GPUDataUploader& gpu_data_uploader,
		const Tonemapper& tonemapper);

	vk::ImageView GetAmbientOcclusionImageView() const;

	void DoPass(vk::CommandBuffer command_buffer);

	~AmbientOcclusionCalculator();

private:
	Settings& settings_;
	const vk::Device vk_device_;

	vk::Extent2D framebuffer_size_;
	vk::UniqueImage framebuffer_image_;
	vk::UniqueDeviceMemory framebuffer_image_memory_;
	vk::UniqueImageView framebuffer_image_view_;

	vk::UniqueRenderPass render_pass_;

	vk::UniqueFramebuffer framebuffer_;

	vk::UniqueImage random_vectors_image_;
	vk::UniqueDeviceMemory random_vectors_image_memory_;

	vk::UniqueDescriptorPool descriptor_pool_;

	vk::UniqueShaderModule shader_vert_;
	vk::UniqueShaderModule shader_frag_;
	vk::UniqueSampler image_sampler_;
	vk::UniqueDescriptorSetLayout descriptor_set_layout_;
	vk::UniquePipelineLayout pipeline_layout_;
	vk::UniquePipeline pipeline_;

	vk::UniqueDescriptorSet descriptor_set_;
};

} // namespace KK
