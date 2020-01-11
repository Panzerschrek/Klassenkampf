#pragma once
#include <vulkan/vulkan.hpp>
#include <vector>


namespace KK
{

class WorldRenderer final
{
public:
	WorldRenderer(
		vk::Device vk_device,
		vk::Format surface_format,
		vk::Extent2D viewport_size,
		const vk::PhysicalDeviceMemoryProperties& memory_properties,
		const std::vector<vk::UniqueImageView>& swapchain_image_views);

	~WorldRenderer();

	void Draw(vk::CommandBuffer command_buffer, size_t swapchain_image_index, float frame_time_s);

private:
	const vk::Device vk_device_;
	const vk::Extent2D viewport_size_;

	vk::UniqueRenderPass vk_render_pass_;
	std::vector<vk::UniqueFramebuffer> vk_framebuffers_;

	vk::UniqueShaderModule shader_vert_;
	vk::UniqueShaderModule shader_frag_;
	vk::UniqueDescriptorSetLayout vk_decriptor_set_layout_;
	vk::UniquePipelineLayout vk_pipeline_layout_;
	vk::UniquePipeline vk_pipeline_;

	vk::UniqueBuffer vk_vertex_buffer_;
	vk::UniqueDeviceMemory vk_vertex_buffer_memory_;

	vk::UniqueBuffer vk_index_buffer_;
	vk::UniqueDeviceMemory vk_index_buffer_memory_;
};

} // namespace KK
