#pragma once
#include "../Common/MemoryMappedFile.hpp"
#include "../Common/SegmentModelFormat.hpp"
#include "../MathLib/Mat.hpp"
#include "GPUDataUploader.hpp"
#include "Tonemapper.hpp"
#include "WindowVulkan.hpp"
#include "WorldGenerator.hpp"
#include <optional>
#include <string>


namespace KK
{

class WorldRenderer final
{
public:
	WorldRenderer(
		WindowVulkan& window_vulkan,
		GPUDataUploader& gpu_data_uploader,
		WorldData::World world);

	~WorldRenderer();

	void BeginFrame(vk::CommandBuffer command_buffer, const m_Mat4& view_matrix);
	void EndFrame(vk::CommandBuffer command_buffer);

private:
	struct Material
	{
		vk::UniqueImage image;
		vk::UniqueImageView image_view;
		vk::UniqueDeviceMemory image_memory;

		vk::UniqueDescriptorSet descriptor_set;
	};

	struct SegmentModelPreloaded
	{
		MemoryMappedFilePtr file_mapped;
		const SegmentModelFormat::SegmentModelHeader& header;
		const SegmentModelFormat::Vertex* vetices;
		const SegmentModelFormat::IndexType* indices;
		const SegmentModelFormat::TriangleGroup* triangle_groups;
		std::vector<std::string> local_to_global_material_id;
	};

	struct Sector
	{
		struct TriangleGroup
		{
			uint32_t first_index;
			uint32_t index_count;
			std::string material_id;
		};

		uint32_t first_vertex;
		std::vector<TriangleGroup> triangle_groups;
	};

	using WorldSectors = std::vector<Sector>;

private:
	void DrawFunction(vk::CommandBuffer command_buffer, const m_Mat4& view_matrix);

	std::optional<SegmentModelPreloaded> PreloadSegmentModel(std::string_view file_name);

	void LoadMaterial(const std::string& material_name);

private:
	GPUDataUploader& gpu_data_uploader_;
	const vk::Device vk_device_;
	const vk::Extent2D viewport_size_;
	const vk::PhysicalDeviceMemoryProperties memory_properties_;
	const uint32_t queue_family_index_;

	const WorldData::World world_;

	Tonemapper tonemapper_;

	vk::UniqueShaderModule shader_vert_;
	vk::UniqueShaderModule shader_frag_;
	vk::UniqueDescriptorSetLayout vk_decriptor_set_layout_;
	vk::UniquePipelineLayout vk_pipeline_layout_;
	vk::UniquePipeline vk_pipeline_;

	vk::UniqueDescriptorPool vk_descriptor_pool_;

	WorldSectors world_sectors_;
	vk::UniqueBuffer vk_vertex_buffer_;
	vk::UniqueDeviceMemory vk_vertex_buffer_memory_;
	vk::UniqueBuffer vk_index_buffer_;
	size_t index_count_= 0u;
	vk::UniqueDeviceMemory vk_index_buffer_memory_;

	vk::UniqueSampler vk_image_sampler_;

	std::unordered_map<std::string, Material> materials_;

	std::string test_material_id_;
};

} // namespace KK
