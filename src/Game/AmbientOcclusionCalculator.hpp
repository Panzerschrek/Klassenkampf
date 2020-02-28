#pragma once
#include "GPUDataUploader.hpp"
#include "CameraController.hpp"
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

	void DoPass(vk::CommandBuffer command_buffer, const CameraController::ViewMatrix& view_matrix);

	~AmbientOcclusionCalculator();

private:
	struct PassData
	{
		vk::UniqueImage framebuffer_image;
		vk::UniqueDeviceMemory framebuffer_image_memory;
		vk::UniqueImageView framebuffer_image_view;
		vk::UniqueFramebuffer framebuffer;
	};

	struct Pipeline
	{

		vk::UniqueShaderModule shader_vert;
		vk::UniqueShaderModule shader_frag;
		std::vector<vk::UniqueSampler> samplers;
		vk::UniqueDescriptorSetLayout descriptor_set_layout;
		vk::UniquePipelineLayout pipeline_layout;
		vk::UniquePipeline pipeline;
	};

private:
	Pipeline CreateSSAOPipeline();
	Pipeline CreateBlurPipeline();

private:
	Settings& settings_;
	const vk::Device vk_device_;

	vk::Extent2D framebuffer_size_;
	vk::UniqueRenderPass render_pass_;

	vk::UniqueImage random_vectors_image_;
	vk::UniqueDeviceMemory random_vectors_image_memory_;
	vk::UniqueImageView random_vectors_image_view_;

	// 0 - ambient occlusion pass calculate, 2 - blur pass
	PassData pass_data_[2];
	Pipeline ssao_pipeline_;
	Pipeline blur_pipeline_;

	vk::UniqueDescriptorPool descriptor_pool_;
	vk::UniqueDescriptorSet ssao_descriptor_set_;
	vk::UniqueDescriptorSet blur_descriptor_set_;

};

} // namespace KK
