#pragma once
#include "MathLib/Mat.hpp"
#include "WindowVulkan.hpp"


namespace KK
{

class WorldRenderer final
{
public:
	explicit WorldRenderer(WindowVulkan& window_vulkan);

	~WorldRenderer();

	void BeginFrame(vk::CommandBuffer command_buffer);
	void EndFrame(vk::CommandBuffer command_buffer, const m_Mat4& view_matrix);

private:
	const vk::Device vk_device_;

	vk::UniqueShaderModule shader_vert_;
	vk::UniqueShaderModule shader_frag_;
	vk::UniqueDescriptorSetLayout vk_decriptor_set_layout_;
	vk::UniquePipelineLayout vk_pipeline_layout_;
	vk::UniquePipeline vk_pipeline_;

	vk::UniqueDescriptorPool vk_descriptor_pool_;
	vk::UniqueDescriptorSet vk_descriptor_set_;

	vk::UniqueBuffer vk_vertex_buffer_;
	vk::UniqueDeviceMemory vk_vertex_buffer_memory_;

	vk::UniqueBuffer vk_index_buffer_;
	vk::UniqueDeviceMemory vk_index_buffer_memory_;

	vk::UniqueImage vk_image_;
	vk::UniqueDeviceMemory vk_image_memory_;
	vk::UniqueImageView vk_image_view_;
	vk::UniqueSampler vk_image_sampler_;
};

} // namespace KK
