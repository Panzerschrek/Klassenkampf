#pragma once
#include "../Common/MemoryMappedFile.hpp"
#include "../Common/SegmentModelFormat.hpp"
#include "../MathLib/Mat.hpp"
#include "AmbientOcclusionCalculator.hpp"
#include "CameraController.hpp"
#include "CommandsProcessor.hpp"
#include "ClusterVolumeBuilder.hpp"
#include "GPUDataUploader.hpp"
#include "Shadowmapper.hpp"
#include "ShadowmapAllocator.hpp"
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
		Settings& settings,
		CommandsProcessor& command_processor,
		WindowVulkan& window_vulkan,
		GPUDataUploader& gpu_data_uploader,
	const CameraController& camera_controller,
		const WorldData::World& world);

	~WorldRenderer();

	void BeginFrame(vk::CommandBuffer command_buffer);
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
			float radius;
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

	using VisibleSectors= std::vector<size_t>;

	struct Pipeline
	{
		vk::UniqueShaderModule shader_vert;
		vk::UniqueShaderModule shader_frag;
		vk::UniqueSampler image_sampler;
		vk::UniqueSampler depth_cubemap_image_sampler;
		vk::UniqueDescriptorSetLayout descriptor_set_layout;
		vk::UniquePipelineLayout pipeline_layout;
		vk::UniquePipeline pipeline;
	};

private:
	Pipeline CreateDepthPrePassPipeline();
	Pipeline CreateLightingPassPipeline();

	void DrawWorldModelDepthPrePass(
		vk::CommandBuffer command_buffer,
		const WorldModel& world_model,
		const VisibleSectors& visible_sectors,
		const m_Mat4& view_matrix);

	void DrawWorldModelMainPass(
		vk::CommandBuffer command_buffer,
		const WorldModel& world_model,
		const VisibleSectors& visible_sectors,
		const m_Mat4& view_matrix);

	void DrawWorldModelToDepthCubemap(
		vk::CommandBuffer command_buffer,
		const WorldModel& world_model,
		const m_Vec3& light_pos,
		float light_radius);

	WorldModel LoadWorld(const WorldData::World& world, const SegmentModels& segment_models);
	std::optional<SegmentModel> LoadSegmentModel(std::string_view file_name);

	void LoadMaterial(const std::string& material_name);

	void ComandTestLightAdd(const CommandsArguments& args);
	void CommandTestLightRemove();

private:
	GPUDataUploader& gpu_data_uploader_;
	const CameraController& camera_controller_;
	const vk::Device vk_device_;
	const vk::Extent2D viewport_size_;
	const vk::PhysicalDeviceMemoryProperties memory_properties_;
	const uint32_t queue_family_index_;

	CommandsMapConstPtr commands_map_;

	Tonemapper tonemapper_;
	AmbientOcclusionCalculator ambient_occlusion_culculator_;
	Shadowmapper shadowmapper_;
	ClusterVolumeBuilder cluster_volume_builder_;
	ShadowmapAllocator shadowmap_allocator_;

	Pipeline depth_pre_pass_pipeline_;
	Pipeline lighting_pass_pipeline_;

	// All light data, required by fragment shader - ligh sources with parameters (position, matrix), etc.
	vk::UniqueBuffer vk_light_data_buffer_;
	vk::UniqueDeviceMemory vk_light_data_buffer_memory_;

	// 3D table of offsets to lights list for each cluster.
	vk::UniqueBuffer cluster_offset_buffer_;
	vk::UniqueDeviceMemory cluster_offset_buffer_memory_;

	// List of light sources for each cluster.
	size_t lights_list_buffer_size_= 0u; // In elements
	vk::UniqueBuffer lights_list_buffer_;
	vk::UniqueDeviceMemory lights_list_buffer_memory_;

	vk::UniqueDescriptorPool vk_descriptor_pool_;

	WorldModel world_model_;
	WorldModel test_world_model_;

	std::unordered_map<std::string, Material> materials_;
	std::string test_material_id_;

	std::optional<Sector::Light> test_light_;
};

} // namespace KK
