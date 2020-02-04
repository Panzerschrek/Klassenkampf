#pragma once
#include "../MathLib/Mat.hpp"
#include "GPUDataUploader.hpp"
#include "Tonemapper.hpp"
#include "WindowVulkan.hpp"
#include "WorldGenerator.hpp"
#include <optional>


namespace KK
{

class WorldRenderer final
{
public:
	WorldRenderer(
		WindowVulkan& window_vulkan,
		GPUDataUploader& gpu_data_uploader,
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
			size_t material_index;
		};

		struct Material
		{
			vk::UniqueImage image;
			vk::UniqueImageView image_view;
			vk::UniqueDeviceMemory image_memory;

			vk::UniqueDescriptorSet descriptor_set;
		};

		std::vector<TriangleGroup> triangle_groups;
		std::vector<Material> materials;

		vk::UniqueBuffer vertex_buffer;
		vk::UniqueDeviceMemory vertex_buffer_memory;

		vk::UniqueBuffer index_buffer;
		vk::UniqueDeviceMemory index_buffer_memory;
	};

private:
	std::optional<SegmentModel> LoadSegmentModel(const char* const file_name);

private:
	GPUDataUploader& gpu_data_uploader_;
	const vk::Device vk_device_;
	const vk::Extent2D viewport_size_;
	const vk::PhysicalDeviceMemoryProperties memory_properties_;
	const uint32_t queue_family_index_;

	Tonemapper tonemapper_;

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

	vk::UniqueBuffer vk_uniform_buffer_;
	vk::UniqueDeviceMemory vk_uniform_buffer_memory_;

	std::vector<SegmentModel> segment_models_;
};

} // namespace KK
