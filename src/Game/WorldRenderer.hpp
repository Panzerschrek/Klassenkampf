#pragma once
#include "../MathLib/Mat.hpp"
#include "WindowVulkan.hpp"
#include "WorldGenerator.hpp"


namespace KK
{

class WorldRenderer final
{
public:
	WorldRenderer(
		WindowVulkan& window_vulkan,
		const WorldData::World& world);

	~WorldRenderer();

	void BeginFrame(vk::CommandBuffer command_buffer, const m_Mat4& view_matrix);
	void EndFrame(vk::CommandBuffer command_buffer);

private:
	struct SegmentModel
	{

		struct TriangleGroup
		{
			uint32_t first_vertex;
			uint32_t first_index;
			uint32_t index_count;
		};

		std::vector<TriangleGroup> triangle_groups;

		vk::UniqueBuffer vertex_buffer;
		vk::UniqueDeviceMemory vertex_buffer_memory;

		vk::UniqueBuffer index_buffer;
		vk::UniqueDeviceMemory index_buffer_memory;
	};

private:
	SegmentModel LoadSegmentModel(const char* const file_name);

private:
	const vk::Device vk_device_;
	const vk::Extent2D viewport_size_;
	const vk::PhysicalDeviceMemoryProperties memory_properties_;

	vk::Extent2D framebuffer_size_;
	vk::UniqueImage framebuffer_image_;
	vk::UniqueDeviceMemory framebuffer_image_memory_;
	vk::UniqueImageView framebuffer_image_view_;
	vk::UniqueSampler framebuffer_image_sampler_;

	vk::UniqueImage framebuffer_depth_image_;
	vk::UniqueDeviceMemory framebuffer_depth_image_memory_;
	vk::UniqueImageView framebuffer_depth_image_view_;

	vk::UniqueRenderPass framebuffer_render_pass_;
	vk::UniqueFramebuffer framebuffer_;

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
	size_t index_count_= 0u;
	vk::UniqueDeviceMemory vk_index_buffer_memory_;

	vk::UniqueImage vk_image_;
	vk::UniqueDeviceMemory vk_image_memory_;
	vk::UniqueImageView vk_image_view_;
	vk::UniqueSampler vk_image_sampler_;

	vk::UniqueShaderModule tonemapping_shader_vert_;
	vk::UniqueShaderModule tonemapping_shader_frag_;
	vk::UniqueDescriptorSetLayout tonemapping_decriptor_set_layout_;
	vk::UniquePipelineLayout tonemapping_pipeline_layout_;
	vk::UniquePipeline tonemapping_pipeline_;

	vk::UniqueDescriptorPool tonemapping_descriptor_pool_;
	vk::UniqueDescriptorSet tonemapping_descriptor_set_;

	SegmentModel segment_model_;
};

} // namespace KK
