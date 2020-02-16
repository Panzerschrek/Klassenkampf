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
		const WorldData::World& world);

	~WorldRenderer();

	void BeginFrame(vk::CommandBuffer command_buffer, const m_Mat4& view_matrix, const m_Vec3& cam_pos);
	void EndFrame(vk::CommandBuffer command_buffer);

private:
	struct Material
	{
		vk::UniqueImage image;
		vk::UniqueImageView image_view;
		vk::UniqueDeviceMemory image_memory;

		vk::UniqueDescriptorSet descriptor_set;
	};

	struct SegmentModel
	{
		MemoryMappedFilePtr file_mapped;
		const SegmentModelFormat::SegmentModelHeader& header;
		const SegmentModelFormat::Vertex* vetices;
		const SegmentModelFormat::IndexType* indices;
		const SegmentModelFormat::TriangleGroup* triangle_groups;
		const SegmentModelFormat::Light* lights;
		std::vector<std::string> local_to_global_material_id;
	};

	using SegmentModels= std::unordered_map<WorldData::SegmentType, SegmentModel>;

	struct Sector
	{
		struct TriangleGroup
		{
			uint32_t first_vertex;
			uint32_t first_index;
			uint32_t index_count;
			std::string material_id;
		};

		struct Light
		{
			m_Vec3 pos;
			m_Vec3 color;
		};

		m_Vec3 bb_min;
		m_Vec3 bb_max;
		std::vector<TriangleGroup> triangle_groups;
		std::vector<Light> lights;
	};

	using WorldSectors= std::vector<Sector>;

	struct WorldModel
	{
		WorldSectors world_sectors_;
		vk::UniqueBuffer vertex_buffer;
		vk::UniqueDeviceMemory vertex_buffer_memory;
		vk::UniqueBuffer index_buffer;
		vk::UniqueDeviceMemory index_buffer_memory;
		WorldSectors sectors;
	};

private:
	void DrawWorldModel(
		vk::CommandBuffer command_buffer,
		const WorldModel& world_model,
		const m_Mat4& view_matrix,
		const m_Vec3& cam_pos);

	WorldModel LoadWorld(const WorldData::World& world, const SegmentModels& segment_models);
	std::optional<SegmentModel> LoadSegmentModel(std::string_view file_name);

	void LoadMaterial(const std::string& material_name);

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

	// All light data, required by fragment shader - ligh sources with parameters (position, matrix), etc.
	vk::UniqueBuffer vk_light_data_buffer_;
	vk::UniqueDeviceMemory vk_light_data_buffer_memory_;

	vk::UniqueDescriptorPool vk_descriptor_pool_;

	WorldModel world_model_;
	WorldModel test_world_model_;

	vk::UniqueSampler vk_image_sampler_;

	std::unordered_map<std::string, Material> materials_;
	std::string test_material_id_;
};

} // namespace KK
