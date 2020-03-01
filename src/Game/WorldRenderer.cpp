#include "WorldRenderer.hpp"
#include "../MathLib/MathConstants.hpp"
#include "Assert.hpp"
#include "DDSImage.hpp"
#include "Image.hpp"
#include "Log.hpp"
#include "ShaderList.hpp"
#include <cmath>
#include <cstring>


namespace KK
{

namespace
{

struct Uniforms
{
	m_Mat4 view_matrix;
};
static_assert(sizeof(Uniforms) <= 128u, "Uniforms size is too big, limit is 128 bytes");

struct LightBuffer
{
	struct Light
	{
		float pos[4];
		float color[4];
		float data[2];
		uint32_t shadowmap_index[2];
	};

	static constexpr size_t c_max_lights= 256u;

	float ambient_color[4];
	uint32_t cluster_volume_size[4];
	float viewport_size[2];
	float w_convert_values[2];
	Light lights[c_max_lights];
};

static_assert(sizeof(LightBuffer::Light) == 48u, "Invalid size");
static_assert(sizeof(LightBuffer) == 48u + LightBuffer::c_max_lights * 48u, "Invalid size");

struct WorldVertex
{
	float pos[3];
	float tex_coord[2];
	int8_t normal[3];
	int8_t binormal[3];
	int8_t tangent[3];
	int8_t reserved[3];
};

const uint32_t g_tex_uniform_binding= 0u;
const uint32_t g_light_buffer_binding= 1u;
const uint32_t g_cluster_offset_buffer_binding= 2u;
const uint32_t g_lights_list_buffer_binding= 3u;
const uint32_t g_depth_cubemaps_array_binding= 4u;
const uint32_t g_ambient_occlusion_buffer_binding= 5u;
const uint32_t g_normals_tex_uniform_binding= 6u;

} // namespace

WorldRenderer::WorldRenderer(
	Settings& settings,
	CommandsProcessor& command_processor,
	WindowVulkan& window_vulkan,
	GPUDataUploader& gpu_data_uploader,
	const CameraController& camera_controller,
	const WorldData::World& world)
	: settings_(settings)
	, gpu_data_uploader_(gpu_data_uploader)
	, camera_controller_(camera_controller)
	, vk_device_(window_vulkan.GetVulkanDevice())
	, viewport_size_(window_vulkan.GetViewportSize())
	, memory_properties_(window_vulkan.GetMemoryProperties())
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, tonemapper_(settings, window_vulkan)
	, ambient_occlusion_culculator_(settings, window_vulkan, gpu_data_uploader, tonemapper_)
	, shadowmapper_(window_vulkan, gpu_data_uploader, sizeof(WorldVertex), offsetof(WorldVertex, pos), vk::Format::eR32G32B32Sfloat)
	, cluster_volume_builder_(16u, 8u, 24u)
	, shadowmap_allocator_(shadowmapper_.GetSize())
{

	commands_map_= std::make_shared<CommandsMap>(
		CommandsMap(
		{
			{ "test_light_add", std::bind(&WorldRenderer::ComandTestLightAdd, this, std::placeholders::_1) },
			{ "test_light_remove", std::bind(&WorldRenderer::CommandTestLightRemove, this) }
		}));
	command_processor.RegisterCommands(commands_map_);

	depth_pre_pass_pipeline_= CreateDepthPrePassPipeline();
	lighting_pass_pipeline_= CreateLightingPassPipeline();

	{ // Prepare lighting buffer.
		vk_light_data_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					sizeof(LightBuffer),
					vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vk_light_data_buffer_);

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties_.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		vk_light_data_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*vk_light_data_buffer_, *vk_light_data_buffer_memory_, 0u);
	}
	{ // Prepare cluster offset buffer.
		const size_t size= cluster_volume_builder_.GetWidth() * cluster_volume_builder_.GetHeight() * cluster_volume_builder_.GetDepth();
		cluster_offset_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					sizeof(uint32_t) * size,
					vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*cluster_offset_buffer_);

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties_.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		cluster_offset_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*cluster_offset_buffer_, *cluster_offset_buffer_memory_, 0u);
	}
	{ // Prepare lights list buffer.
		lights_list_buffer_size_= cluster_volume_builder_.GetWidth() * cluster_volume_builder_.GetHeight() * cluster_volume_builder_.GetDepth() * 32u;
		lights_list_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					sizeof(uint8_t) * lights_list_buffer_size_,
					vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*lights_list_buffer_);

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties_.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		lights_list_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*lights_list_buffer_, *lights_list_buffer_memory_, 0u);
	}

	// Load segment models.
	struct SegmentModelDescription
	{
		WorldData::SegmentType type;
		std::string_view file_name;
	};
	const SegmentModelDescription segment_models_names[]
	{
		{ WorldData::SegmentType::Corridor, "corridor_segment" },
		{ WorldData::SegmentType::Floor, "floor_segment" },
		{ WorldData::SegmentType::Shaft, "shaft_segment" },
		{ WorldData::SegmentType::FloorWallJoint, "floor_wall_join" },
		{ WorldData::SegmentType::FloorWallWallJoint, "floor_wall_wall_join" },
		{ WorldData::SegmentType::Wall, "wall_segment" },
		{ WorldData::SegmentType::CeilingArch4, "arc4_segment" },
		{ WorldData::SegmentType::Column4, "column4_segment" },
		{ WorldData::SegmentType::Column4Lights, "column4_lights" },
	};

	SegmentModels segment_models;
	for(const SegmentModelDescription& segment_model_description : segment_models_names)
	{
		const std::string file_path= "segment_models/" + std::string(segment_model_description.file_name) + ".kks";
		if(std::optional<SegmentModel> model= LoadSegmentModel(file_path))
			segment_models.emplace(segment_model_description.type, std::move(*model));
	}

	// Load materials.
	stub_albedo_image_id_= "albedo_stub";
	LoadImage(stub_albedo_image_id_);
	stub_normal_map_image_id_= "normals_stub";
	LoadImage(stub_normal_map_image_id_);

	world_model_= LoadWorld(world, segment_models);

	// Load test world model.
	{
		SegmentModels test_segment_models;
		if(std::optional<SegmentModel> model= LoadSegmentModel("segment_models/sponza.kks"))
			test_segment_models.emplace(WorldData::SegmentType::Floor, std::move(*model));

		WorldData::World test_world;
		test_world.sectors.emplace_back();

		WorldData::Segment segment;
		segment.pos[0]= 0;
		segment.pos[1]= 0;
		segment.pos[2]= 0;
		segment.type= WorldData::SegmentType::Floor;
		test_world.sectors.back().segments.push_back(std::move(segment));

		test_world_model_= LoadWorld(test_world, test_segment_models);
	}

	gpu_data_uploader_.Flush();

	// Create descriptor set pool.
	const vk::DescriptorPoolSize vk_descriptor_pool_sizes[]
	{
		{
			vk::DescriptorType::eCombinedImageSampler,
			uint32_t(materials_.size() * (2u + shadowmapper_.GetDepthCubemapArrayImagesView().size()))
		},
		{
			vk::DescriptorType::eStorageBuffer,
			uint32_t(materials_.size()) * 3u
		}
	};

	vk_descriptor_pool_=
		vk_device_.createDescriptorPoolUnique(
			vk::DescriptorPoolCreateInfo(
				vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
				uint32_t(materials_.size()), // max sets.
				uint32_t(std::size(vk_descriptor_pool_sizes)), vk_descriptor_pool_sizes));

	for(auto& material_pair : materials_)
	{
		Material& material= material_pair.second;

		// Create descriptor set.
		material.descriptor_set=
			std::move(
			vk_device_.allocateDescriptorSetsUnique(
				vk::DescriptorSetAllocateInfo(
					*vk_descriptor_pool_,
					1u, &*lighting_pass_pipeline_.descriptor_set_layout)).front());

		// Write descriptor set.
		const vk::DescriptorImageInfo descriptor_image_info(
			vk::Sampler(),
			*GetMaterialAlbedoImage(material).image_view,
			vk::ImageLayout::eShaderReadOnlyOptimal);

		const vk::DescriptorBufferInfo descriptor_light_buffer_info(
			*vk_light_data_buffer_,
			0u,
			sizeof(LightBuffer));

		const vk::DescriptorBufferInfo descriptor_offset_buffer_info(
			*cluster_offset_buffer_,
			0u,
			sizeof(uint32_t) * cluster_volume_builder_.GetWidth() * cluster_volume_builder_.GetHeight() * cluster_volume_builder_.GetDepth());

		const vk::DescriptorBufferInfo lights_list_buffer_info(
			*lights_list_buffer_,
			0u,
			sizeof(uint8_t) * lights_list_buffer_size_);

		std::vector<vk::DescriptorImageInfo> descriptor_depth_cubemaps_array_image_infos;
		for(const vk::ImageView& image_view : shadowmapper_.GetDepthCubemapArrayImagesView())
			descriptor_depth_cubemaps_array_image_infos.push_back(
				vk::DescriptorImageInfo(
					vk::Sampler(),
					image_view,
					vk::ImageLayout::eShaderReadOnlyOptimal));

		const vk::DescriptorImageInfo descriptor_ambient_occlusion_image_info(
			vk::Sampler(),
			ambient_occlusion_culculator_.GetAmbientOcclusionImageView(),
			vk::ImageLayout::eShaderReadOnlyOptimal);

		const vk::DescriptorImageInfo descriptor_normals_image_info(
			vk::Sampler(),
			*GetMaterialNormalsImage(material).image_view,
			vk::ImageLayout::eShaderReadOnlyOptimal);

		const vk::WriteDescriptorSet write_descriptor_set[]
		{
			{
				*material.descriptor_set,
				g_tex_uniform_binding,
				0u,
				1u,
				vk::DescriptorType::eCombinedImageSampler,
				&descriptor_image_info,
				nullptr,
				nullptr
			},
			{
				*material.descriptor_set,
				g_light_buffer_binding,
				0u,
				1u,
				vk::DescriptorType::eStorageBuffer,
				nullptr,
				&descriptor_light_buffer_info,
				nullptr
			},
			{
				*material.descriptor_set,
				g_cluster_offset_buffer_binding,
				0u,
				1u,
				vk::DescriptorType::eStorageBuffer,
				nullptr,
				&descriptor_offset_buffer_info,
				nullptr
			},
			{
				*material.descriptor_set,
				g_lights_list_buffer_binding,
				0u,
				1u,
				vk::DescriptorType::eStorageBuffer,
				nullptr,
				&lights_list_buffer_info,
				nullptr
			},
			{
				*material.descriptor_set,
				g_depth_cubemaps_array_binding,
				0u,
				uint32_t(descriptor_depth_cubemaps_array_image_infos.size()),
				vk::DescriptorType::eCombinedImageSampler,
				descriptor_depth_cubemaps_array_image_infos.data(),
				nullptr,
				nullptr
			},
			{
				*material.descriptor_set,
				g_ambient_occlusion_buffer_binding,
				0u,
				1u,
				vk::DescriptorType::eCombinedImageSampler,
				&descriptor_ambient_occlusion_image_info,
				nullptr,
				nullptr
			},
			{
				*material.descriptor_set,
				g_normals_tex_uniform_binding,
				0u,
				1u,
				vk::DescriptorType::eCombinedImageSampler,
				&descriptor_normals_image_info,
				nullptr,
				nullptr
			},
		};

		vk_device_.updateDescriptorSets(
			uint32_t(std::size(write_descriptor_set)), write_descriptor_set,
			0u, nullptr);
	}
}

WorldRenderer::~WorldRenderer()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

void WorldRenderer::BeginFrame(const vk::CommandBuffer command_buffer)
{
	const bool use_test_world_model= settings_.GetInt("test_world_model", 0) != 0;
	settings_.SetInt("test_world_model", use_test_world_model ? 1 : 0);

	const CameraController::ViewMatrix view_matrix= camera_controller_.CalculateViewMatrix();
	const m_Vec3 cam_pos= camera_controller_.GetCameraPosition();
	const WorldModel& model= use_test_world_model ? test_world_model_ : world_model_;

	// Calculate visible sectors.
	const Sector* cam_sector= nullptr;
	for(const Sector& sector : model.sectors)
	{
		if(cam_pos.x >= sector.bb_min.x && cam_pos.x <= sector.bb_max.x &&
			cam_pos.y >= sector.bb_min.y && cam_pos.y <= sector.bb_max.y &&
			cam_pos.z >= sector.bb_min.z && cam_pos.z <= sector.bb_max.z)
		{
			cam_sector= &sector;
			break;
		}
	}

	VisibleSectors visible_sectors;
	for(const Sector& sector : model.sectors)
	{
		if(cam_sector != nullptr)
		{
			// Find adjacent sectors.
			const bool intersects= !(
				sector.bb_min.x > cam_sector->bb_max.x ||
				sector.bb_min.y > cam_sector->bb_max.y ||
				sector.bb_min.z > cam_sector->bb_max.z ||
				sector.bb_max.x < cam_sector->bb_min.x ||
				sector.bb_max.y < cam_sector->bb_min.y ||
				sector.bb_max.z < cam_sector->bb_min.z );
			if(!intersects)
				continue;
		}

		visible_sectors.push_back(size_t(&sector - model.sectors.data()));
	}

	// Prepare light.
	LightBuffer light_buffer;
	light_buffer.ambient_color[0]= 0.1f;
	light_buffer.ambient_color[1]= 0.1f;
	light_buffer.ambient_color[2]= 0.1f;
	light_buffer.ambient_color[3]= 0.0f;
	light_buffer.cluster_volume_size[0]= cluster_volume_builder_.GetWidth ();
	light_buffer.cluster_volume_size[1]= cluster_volume_builder_.GetHeight();
	light_buffer.cluster_volume_size[2]= cluster_volume_builder_.GetDepth ();
	light_buffer.cluster_volume_size[3]= 0;
	light_buffer.viewport_size[0]= float(tonemapper_.GetFramebufferSize().width );
	light_buffer.viewport_size[1]= float(tonemapper_.GetFramebufferSize().height);

	cluster_volume_builder_.ClearClusters();
	cluster_volume_builder_.SetMatrix(view_matrix.mat, view_matrix.z_near, view_matrix.z_far);

	light_buffer.w_convert_values[0]= cluster_volume_builder_.GetWConvertValues().x;
	light_buffer.w_convert_values[1]= cluster_volume_builder_.GetWConvertValues().y;

	uint32_t light_count= 0u;
	std::vector<ShadowmapLight> shadowmap_lights;

	if(test_light_ != std::nullopt)
	{
		const bool added=
			cluster_volume_builder_.AddSphere(
				test_light_->pos,
				test_light_->radius,
				ClusterVolumeBuilder::ElementId(light_count));
		if(added)
		{
			LightBuffer::Light& out_light= light_buffer.lights[light_count];
			out_light.pos[0]= test_light_->pos.x;
			out_light.pos[1]= test_light_->pos.y;
			out_light.pos[2]= test_light_->pos.z;
			out_light.pos[3]= 1.0f / (test_light_->radius * test_light_->radius); // Fade to zero at radius.
			out_light.color[0]= test_light_->color.x;
			out_light.color[1]= test_light_->color.y;
			out_light.color[2]= test_light_->color.z;
			out_light.color[3]= 0.0f;
			out_light.data[0]= 1.0f / test_light_->radius;
			out_light.data[1]= 0.0f;
			out_light.shadowmap_index[0]= 0;
			out_light.shadowmap_index[1]= 0;

			ShadowmapLight shadowmap_light;
			shadowmap_light.pos= test_light_->pos;
			shadowmap_light.radius= test_light_->radius;
			shadowmap_lights.push_back(shadowmap_light);

			++light_count;
		}
	}

	for(const size_t sector_index : visible_sectors)
	for(const Sector::Light& sector_light : model.sectors[sector_index].lights)
	{
		if(light_count >= LightBuffer::c_max_lights)
			goto end_fill_lights;

		const bool added=
			cluster_volume_builder_.AddSphere(
				sector_light.pos,
				sector_light.radius,
				ClusterVolumeBuilder::ElementId(light_count));
		if(!added)
			continue;

		LightBuffer::Light& out_light= light_buffer.lights[light_count];
		out_light.pos[0]= sector_light.pos.x;
		out_light.pos[1]= sector_light.pos.y;
		out_light.pos[2]= sector_light.pos.z;
		out_light.pos[3]= 1.0f / (sector_light.radius * sector_light.radius); // Fade to zero at radius.
		out_light.color[0]= sector_light.color.x;
		out_light.color[1]= sector_light.color.y;
		out_light.color[2]= sector_light.color.z;
		out_light.color[3]= 0.0f;
		out_light.data[0]= 1.0f / sector_light.radius;
		out_light.data[1]= 0.0f;
		out_light.shadowmap_index[0]= 0;
		out_light.shadowmap_index[1]= 0;

		ShadowmapLight shadowmap_light;
		shadowmap_light.pos= sector_light.pos;
		shadowmap_light.radius= sector_light.radius;
		shadowmap_lights.push_back(shadowmap_light);

		++light_count;
	}
	end_fill_lights:

	KK_ASSERT(light_count == shadowmap_lights.size());

	// Allocate shadowmaps.
	const ShadowmapAllocator::LightsForShadowUpdate lights_for_shadow_update=
		shadowmap_allocator_.UpdateLights(shadowmap_lights, cam_pos);
	for(uint32_t i= 0u; i < light_count; ++i)
	{
		const auto slot= shadowmap_allocator_.GetLightShadowmapSlot(shadowmap_lights[i]);
		light_buffer.lights[i].shadowmap_index[0]= slot.first;
		light_buffer.lights[i].shadowmap_index[1]= slot.second;
	}

	const auto& clusters= cluster_volume_builder_.GetClusters();
	std::vector<ClusterVolumeBuilder::ElementId> ligts_list_buffer;
	std::vector<uint32_t> offsets_buffer(clusters.size());
	for(size_t i= 0u; i < clusters.size(); ++i)
	{
		offsets_buffer[i]= uint32_t(ligts_list_buffer.size());
		ligts_list_buffer.push_back(ClusterVolumeBuilder::ElementId(clusters[i].elements.size()));
		ligts_list_buffer.insert(
			ligts_list_buffer.end(),
			clusters[i].elements.begin(), clusters[i].elements.end());
	}

	// Vulkan requires multiple of 4 sizes.
	ligts_list_buffer.resize((ligts_list_buffer.size() + 3u) & ~3u);

	command_buffer.updateBuffer(
		*vk_light_data_buffer_,
		0u,
		offsetof(LightBuffer, lights) + sizeof(LightBuffer::Light) * light_count, // Update only visible lights.
		&light_buffer);
	command_buffer.updateBuffer(*cluster_offset_buffer_, 0u, uint32_t(offsets_buffer.size() * sizeof(uint32_t)), offsets_buffer.data());
	command_buffer.updateBuffer(*lights_list_buffer_, 0u, uint32_t(ligts_list_buffer.size() * sizeof(ClusterVolumeBuilder::ElementId)), ligts_list_buffer.data());

	// Draw shadows
	for(const ShadowmapLight& light : lights_for_shadow_update)
	{
		const ShadowmapSlot slot= shadowmap_allocator_.GetLightShadowmapSlot(light);
		if(slot == ShadowmapAllocator::c_invalid_shadowmap_slot)
			continue;

		shadowmapper_.DrawToDepthCubemap(
			command_buffer,
			slot,
			light.pos,
			light.radius,
			[&]{ DrawWorldModelToDepthCubemap(command_buffer, model, light.pos, light.radius); } );
	}

	// Draw
	tonemapper_.DeDepthPrePass(
		command_buffer,
		[&]{ DrawWorldModelDepthPrePass(command_buffer, model, visible_sectors, view_matrix.mat); });

	ambient_occlusion_culculator_.DoPass(command_buffer, view_matrix);

	tonemapper_.DoMainPass(
		command_buffer,
		[&]{ DrawWorldModelMainPass(command_buffer, model, visible_sectors, view_matrix.mat); });
}

void WorldRenderer::EndFrame(const vk::CommandBuffer command_buffer)
{
	tonemapper_.EndFrame(command_buffer);
}

WorldRenderer::Pipeline WorldRenderer::CreateDepthPrePassPipeline()
{
	Pipeline pipeline;

	pipeline.shader_vert= CreateShader(vk_device_, ShaderNames::world_depth_only_vert);

	const vk::PushConstantRange vk_push_constant_range(
		vk::ShaderStageFlagBits::eVertex,
		0u,
		sizeof(Uniforms));

	pipeline.pipeline_layout=
		vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				0u, nullptr,
				1u, &vk_push_constant_range));

	// Create pipeline.
	const vk::PipelineShaderStageCreateInfo vk_shader_stage_create_info[]
	{
		{
			vk::PipelineShaderStageCreateFlags(),
			vk::ShaderStageFlagBits::eVertex,
			*pipeline.shader_vert,
			"main"
		},
	};

	const vk::VertexInputBindingDescription vk_vertex_input_binding_description(
		0u,
		sizeof(WorldVertex),
		vk::VertexInputRate::eVertex);

	const vk::VertexInputAttributeDescription vk_vertex_input_attribute_description[]
	{
		{0u, 0u, vk::Format::eR32G32B32Sfloat, offsetof(WorldVertex, pos)},
	};

	const vk::PipelineVertexInputStateCreateInfo vk_pipiline_vertex_input_state_create_info(
		vk::PipelineVertexInputStateCreateFlags(),
		1u, &vk_vertex_input_binding_description,
		uint32_t(std::size(vk_vertex_input_attribute_description)), vk_vertex_input_attribute_description);

	const vk::PipelineInputAssemblyStateCreateInfo vk_pipeline_input_assembly_state_create_info(
		vk::PipelineInputAssemblyStateCreateFlags(),
		vk::PrimitiveTopology::eTriangleList);

	const vk::Extent2D framebuffer_size= tonemapper_.GetFramebufferSize();
	const vk::Viewport vk_viewport(0.0f, 0.0f, float(framebuffer_size.width), float(framebuffer_size.height), 0.0f, 1.0f);
	const vk::Rect2D vk_scissor(vk::Offset2D(0, 0), framebuffer_size);

	const vk::PipelineViewportStateCreateInfo vk_pipieline_viewport_state_create_info(
		vk::PipelineViewportStateCreateFlags(),
		1u, &vk_viewport,
		1u, &vk_scissor);

	const vk::PipelineRasterizationStateCreateInfo vk_pipilane_rasterization_state_create_info(
		vk::PipelineRasterizationStateCreateFlags(),
		VK_FALSE,
		VK_FALSE,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eBack,
		vk::FrontFace::eCounterClockwise,
		VK_FALSE, 0.0f, 0.0f, 0.0f,
		1.0f);

	const vk::PipelineMultisampleStateCreateInfo vk_pipeline_multisample_state_create_info(
		vk::PipelineMultisampleStateCreateFlags(),
		tonemapper_.GetSampleCount());

	const vk::PipelineDepthStencilStateCreateInfo vk_pipeline_depth_state_create_info(
		vk::PipelineDepthStencilStateCreateFlags(),
		VK_TRUE,
		VK_TRUE,
		vk::CompareOp::eLess,
		VK_FALSE,
		VK_FALSE,
		vk::StencilOpState(),
		vk::StencilOpState(),
		0.0f,
		1.0f);

	const vk::PipelineColorBlendAttachmentState vk_pipeline_color_blend_attachment_state(
		VK_FALSE,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
		vk::ColorComponentFlags() /* do not write to color buffer */);

	const vk::PipelineColorBlendStateCreateInfo vk_pipeline_color_blend_state_create_info(
		vk::PipelineColorBlendStateCreateFlags(),
		VK_FALSE,
		vk::LogicOp::eCopy,
		1u, &vk_pipeline_color_blend_attachment_state);

	pipeline.pipeline=
		vk_device_.createGraphicsPipelineUnique(
			nullptr,
			vk::GraphicsPipelineCreateInfo(
				vk::PipelineCreateFlags(),
				uint32_t(std::size(vk_shader_stage_create_info)),
				vk_shader_stage_create_info,
				&vk_pipiline_vertex_input_state_create_info,
				&vk_pipeline_input_assembly_state_create_info,
				nullptr,
				&vk_pipieline_viewport_state_create_info,
				&vk_pipilane_rasterization_state_create_info,
				&vk_pipeline_multisample_state_create_info,
				&vk_pipeline_depth_state_create_info,
				&vk_pipeline_color_blend_state_create_info,
				nullptr,
				*pipeline.pipeline_layout,
				tonemapper_.GetDepthPrePass(),
				0u));

	return pipeline;
}

WorldRenderer::Pipeline WorldRenderer::CreateLightingPassPipeline()
{
	Pipeline pipeline;

	// Create shaders
	pipeline.shader_vert= CreateShader(vk_device_, ShaderNames::world_vert);
	pipeline.shader_frag= CreateShader(vk_device_, ShaderNames::world_frag);

	// Create image samplers
	// Albedo sampler.
	pipeline.samplers.push_back(
		vk_device_.createSamplerUnique(
			vk::SamplerCreateInfo(
				vk::SamplerCreateFlags(),
				vk::Filter::eLinear,
				vk::Filter::eLinear,
				vk::SamplerMipmapMode::eLinear,
				vk::SamplerAddressMode::eRepeat,
				vk::SamplerAddressMode::eRepeat,
				vk::SamplerAddressMode::eRepeat,
				0.0f,
				VK_TRUE, // anisotropy
				4.0f, // anisotropy level
				VK_FALSE,
				vk::CompareOp::eNever,
				0.0f,
				100.0f,
				vk::BorderColor::eFloatTransparentBlack,
				VK_FALSE)));

	// Depth cubemap sampler.
	pipeline.samplers.push_back(
		vk_device_.createSamplerUnique(
			vk::SamplerCreateInfo(
				vk::SamplerCreateFlags(),
				vk::Filter::eLinear,
				vk::Filter::eLinear,
				vk::SamplerMipmapMode::eNearest,
				vk::SamplerAddressMode::eClampToEdge,
				vk::SamplerAddressMode::eClampToEdge,
				vk::SamplerAddressMode::eClampToEdge,
				0.0f,
				VK_FALSE,
				0.0f,
				VK_TRUE,
				vk::CompareOp::eLessOrEqual,
				0.0f,
				0.0f,
				vk::BorderColor::eFloatTransparentBlack,
				VK_FALSE)));

	// SSAO sampler.
	pipeline.samplers.push_back(
		vk_device_.createSamplerUnique(
			vk::SamplerCreateInfo(
				vk::SamplerCreateFlags(),
				vk::Filter::eLinear,
				vk::Filter::eLinear,
				vk::SamplerMipmapMode::eNearest,
				vk::SamplerAddressMode::eClampToEdge,
				vk::SamplerAddressMode::eClampToEdge,
				vk::SamplerAddressMode::eClampToEdge,
				0.0f,
				VK_FALSE,
				0.0f,
				VK_FALSE,
				vk::CompareOp::eNever,
				0.0f,
				0.0f,
				vk::BorderColor::eFloatTransparentBlack,
				VK_FALSE)));

	// Normals sampler.
	pipeline.samplers.push_back(
		vk_device_.createSamplerUnique(
			vk::SamplerCreateInfo(
				vk::SamplerCreateFlags(),
				vk::Filter::eLinear,
				vk::Filter::eLinear,
				vk::SamplerMipmapMode::eLinear,
				vk::SamplerAddressMode::eRepeat,
				vk::SamplerAddressMode::eRepeat,
				vk::SamplerAddressMode::eRepeat,
				0.5f, // Increase mip bias for normals
				VK_TRUE, // anisotropy
				4.0f, // anisotropy level
				VK_FALSE,
				vk::CompareOp::eNever,
				0.0f,
				100.0f,
				vk::BorderColor::eFloatTransparentBlack,
				VK_FALSE)));

	const std::vector<vk::Sampler> depth_cubemap_image_samplers(
		shadowmapper_.GetDepthCubemapArrayImagesView().size(),
		*pipeline.samplers[1]);

	// Create pipeline layout
	const vk::DescriptorSetLayoutBinding vk_descriptor_set_layout_bindings[]
	{
		{
			g_tex_uniform_binding,
			vk::DescriptorType::eCombinedImageSampler,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			&*pipeline.samplers[0],
		},
		{
			g_light_buffer_binding,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			nullptr,
		},
		{
			g_cluster_offset_buffer_binding,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			nullptr,
		},
		{
			g_lights_list_buffer_binding,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			nullptr,
		},
		{
			g_depth_cubemaps_array_binding,
			vk::DescriptorType::eCombinedImageSampler,
			uint32_t(depth_cubemap_image_samplers.size()),
			vk::ShaderStageFlagBits::eFragment,
			depth_cubemap_image_samplers.data(),
		},
		{
			g_ambient_occlusion_buffer_binding,
			vk::DescriptorType::eCombinedImageSampler,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			&*pipeline.samplers[2],
		},
		{
			g_normals_tex_uniform_binding,
			vk::DescriptorType::eCombinedImageSampler,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			&*pipeline.samplers[3],
		},
	};

	pipeline.descriptor_set_layout=
		vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(vk_descriptor_set_layout_bindings)), vk_descriptor_set_layout_bindings));

	const vk::PushConstantRange vk_push_constant_range(
		vk::ShaderStageFlagBits::eVertex,
		0u,
		sizeof(Uniforms));

	pipeline.pipeline_layout=
		vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*pipeline.descriptor_set_layout,
				1u, &vk_push_constant_range));

	// Create pipeline.
	const vk::PipelineShaderStageCreateInfo vk_shader_stage_create_info[2]
	{
		{
			vk::PipelineShaderStageCreateFlags(),
			vk::ShaderStageFlagBits::eVertex,
			*pipeline.shader_vert,
			"main"
		},
		{
			vk::PipelineShaderStageCreateFlags(),
			vk::ShaderStageFlagBits::eFragment,
			*pipeline.shader_frag,
			"main"
		},
	};

	const vk::VertexInputBindingDescription vk_vertex_input_binding_description(
		0u,
		sizeof(WorldVertex),
		vk::VertexInputRate::eVertex);

	const vk::VertexInputAttributeDescription vk_vertex_input_attribute_description[]
	{
		{0u, 0u, vk::Format::eR32G32B32Sfloat, offsetof(WorldVertex, pos)},
		{1u, 0u, vk::Format::eR32G32B32Sfloat, offsetof(WorldVertex, tex_coord)},
		{2u, 0u, vk::Format::eR8G8B8A8Snorm, offsetof(WorldVertex, normal)},
		{3u, 0u, vk::Format::eR8G8B8A8Snorm, offsetof(WorldVertex, binormal)},
		{4u, 0u, vk::Format::eR8G8B8A8Snorm, offsetof(WorldVertex, tangent)},
	};

	const vk::PipelineVertexInputStateCreateInfo vk_pipiline_vertex_input_state_create_info(
		vk::PipelineVertexInputStateCreateFlags(),
		1u, &vk_vertex_input_binding_description,
		uint32_t(std::size(vk_vertex_input_attribute_description)), vk_vertex_input_attribute_description);

	const vk::PipelineInputAssemblyStateCreateInfo vk_pipeline_input_assembly_state_create_info(
		vk::PipelineInputAssemblyStateCreateFlags(),
		vk::PrimitiveTopology::eTriangleList);

	const vk::Extent2D framebuffer_size= tonemapper_.GetFramebufferSize();
	const vk::Viewport vk_viewport(0.0f, 0.0f, float(framebuffer_size.width), float(framebuffer_size.height), 0.0f, 1.0f);
	const vk::Rect2D vk_scissor(vk::Offset2D(0, 0), framebuffer_size);

	const vk::PipelineViewportStateCreateInfo vk_pipieline_viewport_state_create_info(
		vk::PipelineViewportStateCreateFlags(),
		1u, &vk_viewport,
		1u, &vk_scissor);

	const vk::PipelineRasterizationStateCreateInfo vk_pipilane_rasterization_state_create_info(
		vk::PipelineRasterizationStateCreateFlags(),
		VK_FALSE,
		VK_FALSE,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eBack,
		vk::FrontFace::eCounterClockwise,
		VK_FALSE, 0.0f, 0.0f, 0.0f,
		1.0f);

	const vk::PipelineMultisampleStateCreateInfo vk_pipeline_multisample_state_create_info(
		vk::PipelineMultisampleStateCreateFlags(),
		tonemapper_.GetSampleCount());

	const vk::PipelineDepthStencilStateCreateInfo vk_pipeline_depth_state_create_info(
		vk::PipelineDepthStencilStateCreateFlags(),
		VK_TRUE,
		VK_FALSE, // Disable depth write.
		vk::CompareOp::eEqual, // Draw only fragments with equal depth.
		VK_FALSE,
		VK_FALSE,
		vk::StencilOpState(),
		vk::StencilOpState(),
		0.0f,
		1.0f);

	const vk::PipelineColorBlendAttachmentState vk_pipeline_color_blend_attachment_state(
		VK_FALSE,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

	const vk::PipelineColorBlendStateCreateInfo vk_pipeline_color_blend_state_create_info(
		vk::PipelineColorBlendStateCreateFlags(),
		VK_FALSE,
		vk::LogicOp::eCopy,
		1u, &vk_pipeline_color_blend_attachment_state);

	pipeline.pipeline=
		vk_device_.createGraphicsPipelineUnique(
			nullptr,
			vk::GraphicsPipelineCreateInfo(
				vk::PipelineCreateFlags(),
				uint32_t(std::size(vk_shader_stage_create_info)),
				vk_shader_stage_create_info,
				&vk_pipiline_vertex_input_state_create_info,
				&vk_pipeline_input_assembly_state_create_info,
				nullptr,
				&vk_pipieline_viewport_state_create_info,
				&vk_pipilane_rasterization_state_create_info,
				&vk_pipeline_multisample_state_create_info,
				&vk_pipeline_depth_state_create_info,
				&vk_pipeline_color_blend_state_create_info,
				nullptr,
				*pipeline.pipeline_layout,
				tonemapper_.GetMainRenderPass(),
				0u));

	return pipeline;
}

void WorldRenderer::DrawWorldModelDepthPrePass(
	const vk::CommandBuffer command_buffer,
	const WorldModel& world_model,
	const VisibleSectors& visible_setors,
	const m_Mat4& view_matrix)
{
	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *depth_pre_pass_pipeline_.pipeline);

	Uniforms uniforms;
	uniforms.view_matrix= view_matrix;
	command_buffer.pushConstants(
		*depth_pre_pass_pipeline_.pipeline_layout,
		vk::ShaderStageFlagBits::eVertex,
		0,
		sizeof(uniforms),
		&uniforms);

	const vk::DeviceSize offsets= 0u;
	command_buffer.bindVertexBuffers(0u, 1u, &*world_model.vertex_buffer, &offsets);
	command_buffer.bindIndexBuffer(*world_model.index_buffer, 0u, vk::IndexType::eUint16);

	for(const size_t sector_index : visible_setors)
	for(const Sector::TriangleGroup& triangle_group : world_model.sectors[sector_index].triangle_groups)
		command_buffer.drawIndexed(triangle_group.index_count, 1u, triangle_group.first_index, triangle_group.first_vertex, 0u);
}

void WorldRenderer::DrawWorldModelMainPass(
	const vk::CommandBuffer command_buffer,
	const WorldModel& world_model,
	const VisibleSectors& visible_setors,
	const m_Mat4& view_matrix)
{
	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *lighting_pass_pipeline_.pipeline);

	Uniforms uniforms;
	uniforms.view_matrix= view_matrix;
	command_buffer.pushConstants(
		*lighting_pass_pipeline_.pipeline_layout,
		vk::ShaderStageFlagBits::eVertex,
		0,
		sizeof(uniforms),
		&uniforms);

	const vk::DeviceSize offsets= 0u;
	command_buffer.bindVertexBuffers(0u, 1u, &*world_model.vertex_buffer, &offsets);
	command_buffer.bindIndexBuffer(*world_model.index_buffer, 0u, vk::IndexType::eUint16);

	for(const size_t sector_index : visible_setors)
	for(const Sector::TriangleGroup& triangle_group : world_model.sectors[sector_index].triangle_groups)
	{
		const Material& material= materials_.find(triangle_group.material_id)->second;

		command_buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			*lighting_pass_pipeline_.pipeline_layout,
			0u,
			1u, &*material.descriptor_set,
			0u, nullptr);

		command_buffer.drawIndexed(triangle_group.index_count, 1u, triangle_group.first_index, triangle_group.first_vertex, 0u);
	}
}

void WorldRenderer::DrawWorldModelToDepthCubemap(
	const vk::CommandBuffer command_buffer,
	const WorldModel& world_model,
	const m_Vec3& light_pos,
	const float light_radius)
{
	const vk::DeviceSize offsets= 0u;
	command_buffer.bindVertexBuffers(0u, 1u, &*world_model.vertex_buffer, &offsets);
	command_buffer.bindIndexBuffer(*world_model.index_buffer, 0u, vk::IndexType::eUint16);

	for(const Sector& sector : world_model.sectors)
	{
		if( light_pos.x + light_radius < sector.bb_min.x ||
			light_pos.x - light_radius > sector.bb_max.x ||
			light_pos.y + light_radius < sector.bb_min.y ||
			light_pos.y - light_radius > sector.bb_max.y ||
			light_pos.z + light_radius < sector.bb_min.z ||
			light_pos.z - light_radius > sector.bb_max.z)
			continue;

		for(const Sector::TriangleGroup& triangle_group : sector.triangle_groups)
			command_buffer.drawIndexed(triangle_group.index_count, 1u, triangle_group.first_index, triangle_group.first_vertex, 0u);
	}
}

WorldRenderer::WorldModel WorldRenderer::LoadWorld(const WorldData::World& world, const SegmentModels& segment_models)
{
	// Combine triangle groups with same material into single triangle groups.
	// Each sector has own set of triangle groups.

	struct SectorTriangleGroup
	{
		std::vector<uint16_t> indices;
		std::vector<WorldVertex> vertcies;
	};

	WorldModel world_model;

	// Create vertex buffer.
	std::vector<WorldVertex> world_vertices;
	std::vector<uint16_t> world_indeces;

	world_model.sectors.resize(world.sectors.size());
	for(size_t s= 0u; s < world_model.sectors.size(); ++s)
	{
		const WorldData::Sector& in_sector= world.sectors[s];
		Sector& out_sector= world_model.sectors[s];

		out_sector.bb_min.x= float(in_sector.bb_min[0]);
		out_sector.bb_min.y= float(in_sector.bb_min[1]);
		out_sector.bb_min.z= float(in_sector.bb_min[2]);
		out_sector.bb_max.x= float(in_sector.bb_max[0]);
		out_sector.bb_max.y= float(in_sector.bb_max[1]);
		out_sector.bb_max.z= float(in_sector.bb_max[2]);

		std::unordered_map< std::string, SectorTriangleGroup > sector_triangle_groups;

		for(const WorldData::Segment& segment : in_sector.segments)
		{
			const auto model_it= segment_models.find(segment.type);
			if(model_it == segment_models.end())
				continue;

			const SegmentModel& model = model_it->second;

			m_Mat4 base_transform_mat, to_center_mat, rotate_mat, from_center_mat, translate_mat, segment_mat;
			base_transform_mat.MakeIdentity();
			base_transform_mat.value[ 0]= model.header.scale[0];
			base_transform_mat.value[ 5]= model.header.scale[1];
			base_transform_mat.value[10]= model.header.scale[2];
			base_transform_mat.value[12]= model.header.shift[0];
			base_transform_mat.value[13]= model.header.shift[1];
			base_transform_mat.value[14]= model.header.shift[2];

			to_center_mat.Translate(m_Vec3(-0.5f, -0.5f, 0.0f));
			rotate_mat.RotateZ(float(segment.angle) * MathConstants::half_pi);
			from_center_mat.Translate(m_Vec3(+0.5f, +0.5f, 0.0f));
			translate_mat.Translate(m_Vec3(float(segment.pos[0]), float(segment.pos[1]), float(segment.pos[2])));
			segment_mat= base_transform_mat * to_center_mat * rotate_mat * from_center_mat * translate_mat;

			for(size_t i= 0u; i < size_t(model.header.triangle_group_count); ++i)
			{
				const SegmentModelFormat::TriangleGroup& in_triangle_group= model.triangle_groups[i];
				SectorTriangleGroup& out_triangle_group= sector_triangle_groups[ model.local_to_global_material_id[in_triangle_group.material_id] ];

				// TODO - maybe also remove duplicated vertices?
				const size_t first_vertex= out_triangle_group.vertcies.size();
				for(size_t j= 0u; j < size_t(in_triangle_group.vertex_count); ++j)
				{
					const SegmentModelFormat::Vertex& in_v= model.vetices[in_triangle_group.first_vertex + j];
					const m_Vec3 pos(float(in_v.pos[0]), float(in_v.pos[1]), float(in_v.pos[2]));
					const m_Vec3 pos_transformed= pos * segment_mat;

					WorldVertex out_v;
					out_v.pos[0]= pos_transformed.x;
					out_v.pos[1]= pos_transformed.y;
					out_v.pos[2]= pos_transformed.z;

					const m_Vec3 normal(float(in_v.normal[0]), float(in_v.normal[1]), float(in_v.normal[2]));
					const m_Vec3 binormal(float(in_v.binormal[0]), float(in_v.binormal[1]), float(in_v.binormal[2]));
					const m_Vec3 tangent(float(in_v.tangent[0]), float(in_v.tangent[1]), float(in_v.tangent[2]));
					const m_Vec3 normal_transformed= normal * rotate_mat;
					const m_Vec3 binormal_transformed= binormal * rotate_mat;
					const m_Vec3 tangent_transformed= tangent * rotate_mat;

					out_v.normal[0]= int8_t(normal_transformed.x);
					out_v.normal[1]= int8_t(normal_transformed.y);
					out_v.normal[2]= int8_t(normal_transformed.z);
					out_v.binormal[0]= int8_t(binormal_transformed.x);
					out_v.binormal[1]= int8_t(binormal_transformed.y);
					out_v.binormal[2]= int8_t(binormal_transformed.z);
					out_v.tangent[0]= int8_t(tangent_transformed.x);
					out_v.tangent[1]= int8_t(tangent_transformed.y);
					out_v.tangent[2]= int8_t(tangent_transformed.z);

					for(size_t j= 0u; j < 2u; ++j)
						out_v.tex_coord[j]= float(in_v.tex_coord[j]) / float(SegmentModelFormat::c_tex_coord_scale);

					out_triangle_group.vertcies.push_back(out_v);
				}

				for(size_t j= 0u; j < in_triangle_group.index_count; ++j)
				{
					const size_t index= model.indices[ in_triangle_group.first_index + j ] + first_vertex;
					KK_ASSERT(index < 65535u);
					out_triangle_group.indices.push_back(uint16_t(index));
				}
			}

			for(size_t i= 0u; i < model.header.light_count; ++i)
			{
				const SegmentModelFormat::Light& in_light= model.lights[i];
				Sector::Light out_light;

				const m_Vec3 pos(float(in_light.pos[0]), float(in_light.pos[1]), float(in_light.pos[2]));
				out_light.pos= pos * segment_mat;
				out_light.radius= in_light.radius * std::max(std::max(model.header.scale[0], model.header.scale[1]), model.header.scale[2]);
				out_light.color= m_Vec3(float(in_light.color[0]), float(in_light.color[1]), float(in_light.color[2])) / 256.0f;

				out_sector.lights.push_back(std::move(out_light));
			}
		} // for sector segments

		for(const auto& triangle_group_pair : sector_triangle_groups)
		{
			const SectorTriangleGroup& triangle_group= triangle_group_pair.second;

			Sector::TriangleGroup out_triangle_group;
			out_triangle_group.material_id= triangle_group_pair.first;
			out_triangle_group.first_vertex= uint32_t(world_vertices.size());
			out_triangle_group.first_index= uint32_t(world_indeces.size());
			out_triangle_group.index_count= uint32_t(triangle_group.indices.size());

			world_vertices.insert(world_vertices.end(), triangle_group.vertcies.begin(), triangle_group.vertcies.end());
			world_indeces.insert(world_indeces.end(), triangle_group.indices.begin(), triangle_group.indices.end());

			out_sector.triangle_groups.push_back(std::move(out_triangle_group));
		}
	} // for sectors

	Log::Info("World sectors: ", world_model.sectors.size());
	Log::Info("World vertices: ", world_vertices.size(), " (", world_vertices.size() * sizeof(WorldVertex) / 1024u / 1024u, "MB)");
	Log::Info("Worl triangles: ", world_indeces.size() / 3u, " (", world_indeces.size() * sizeof(uint16_t) / 1024u / 1024u, "MB)");

	// Create world vertex and index buffers.
	const auto gpu_buffer_upload=
	[&](const void* const buffer_data, const size_t buffer_size, const vk::Buffer dst_buffer)
	{
		size_t offset= 0u;
		const size_t block_size= gpu_data_uploader_.GetMaxMemoryBlockSize();
		while(offset < buffer_size)
		{
			const size_t request_size= std::min(block_size, buffer_size - offset);
			const auto staging_buffer= gpu_data_uploader_.RequestMemory(request_size);

			std::memcpy(
				reinterpret_cast<char*>(staging_buffer.buffer_data) + staging_buffer.buffer_offset,
				reinterpret_cast<const char*>(buffer_data) + offset,
				request_size);

			staging_buffer.command_buffer.copyBuffer(
				staging_buffer.buffer,
				dst_buffer,
				{ vk::BufferCopy(staging_buffer.buffer_offset, offset, request_size) });
			offset+= request_size;
		}
	};

	// Vulkan requires sizes greater, than 0.
	if(world_vertices.empty())
		world_vertices.emplace_back();
	if(world_indeces.empty())
		world_indeces.emplace_back();

	{
		world_model.vertex_buffer=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					world_vertices.size() * sizeof(WorldVertex),
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*world_model.vertex_buffer);

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties_.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		world_model.vertex_buffer_memory= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*world_model.vertex_buffer, *world_model.vertex_buffer_memory, 0u);

		gpu_buffer_upload(world_vertices.data(), world_vertices.size() * sizeof(WorldVertex), *world_model.vertex_buffer);
	}

	{
		world_model.index_buffer=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					world_indeces.size() * sizeof(uint16_t),
					vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*world_model.index_buffer);

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties_.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		world_model.index_buffer_memory= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*world_model.index_buffer, *world_model.index_buffer_memory, 0u);

		gpu_buffer_upload(world_indeces.data(), world_indeces.size() * sizeof(uint16_t), *world_model.index_buffer);
	}

	return world_model;
}

std::optional<WorldRenderer::SegmentModel> WorldRenderer::LoadSegmentModel(const std::string_view file_name)
{
	MemoryMappedFilePtr file_mapped= MemoryMappedFile::Create(file_name);
	if(file_mapped == nullptr)
		return std::nullopt;

	const char* const file_data= static_cast<const char*>(file_mapped->Data());
	const auto& header= *reinterpret_cast<const SegmentModelFormat::SegmentModelHeader*>(file_data);

	if(header.version != SegmentModelFormat::SegmentModelHeader::c_expected_version ||
		std::memcmp(header.header, SegmentModelFormat::SegmentModelHeader::c_expected_header, sizeof(header.header)) != 0)
	{
		Log::Warning("Loading invalid model \"", file_name, "\"");
		return std::nullopt;
	}

	const auto* const in_vertices= reinterpret_cast<const SegmentModelFormat::Vertex*>(file_data + header.vertices_offset);
	const auto* const in_indices= reinterpret_cast<const SegmentModelFormat::IndexType*>(file_data + header.indices_offset);
	const auto* const in_triangle_groups= reinterpret_cast<const SegmentModelFormat::TriangleGroup*>(file_data + header.triangle_groups_offset);
	const auto* const in_materials= reinterpret_cast<const SegmentModelFormat::Material*>(file_data + header.materials_offset);
	const auto* const in_lights= reinterpret_cast<const SegmentModelFormat::Light*>(file_data + header.lights_offset);

	std::vector<std::string> local_to_global_material_id;
	local_to_global_material_id.resize(header.material_count);
	for(size_t i= 0u; i < local_to_global_material_id.size(); ++i)
	{
		local_to_global_material_id[i]= in_materials[i].name;
		LoadMaterial(local_to_global_material_id[i]);
	}

	return
		SegmentModel
		{
			std::move(file_mapped),
			header,
			in_vertices,
			in_indices,
			in_triangle_groups,
			in_lights,
			std::move(local_to_global_material_id)
		};
}

void WorldRenderer::LoadMaterial(const std::string& material_name)
{
	if(materials_.count(material_name) > 0)
		return;

	Material& out_material= materials_[material_name];

	out_material.albedo_image_id= material_name + "-albedo";
	LoadImage(out_material.albedo_image_id);

	out_material.normals_image_id= material_name + "-normals";
	LoadImage(out_material.normals_image_id);
}

void WorldRenderer::LoadImage(const std::string& image_name)
{
	if(materials_.count(image_name) > 0)
		return;

	ImageGPU out_image;

	if(const auto dds_image_opt= DDSImage::Load(("textures/" + image_name + ".dds")))
	{
		const DDSImage& image= *dds_image_opt;
		const std::vector<DDSImage::MipLevel>& mip_levels= image.GetMipLevels();
		if(mip_levels.empty())
			return;

		KK_ASSERT((mip_levels[0].size[0] & (mip_levels[0].size[0] - 1u)) == 0u);
		KK_ASSERT((mip_levels[0].size[1] & (mip_levels[0].size[1] - 1u)) == 0u);

		out_image.image= vk_device_.createImageUnique(
			vk::ImageCreateInfo(
				vk::ImageCreateFlags(),
				vk::ImageType::e2D,
				image.GetFormat(),
				vk::Extent3D(mip_levels[0].size[0], mip_levels[0].size[1], 1u),
				uint32_t(mip_levels.size()),
				1u,
				vk::SampleCountFlagBits::e1,
				vk::ImageTiling::eOptimal,
				vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
				vk::SharingMode::eExclusive,
				0u, nullptr,
				vk::ImageLayout::eUndefined));

		const vk::MemoryRequirements memory_requirements= vk_device_.getImageMemoryRequirements(*out_image.image);

		vk::MemoryAllocateInfo memory_allocate_info(memory_requirements.size);
		for(uint32_t j= 0u; j < memory_properties_.memoryTypeCount; ++j)
		{
			if((memory_requirements.memoryTypeBits & (1u << j)) != 0 &&
				(memory_properties_.memoryTypes[j].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				memory_allocate_info.memoryTypeIndex= j;
		}

		out_image.image_memory= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindImageMemory(*out_image.image, *out_image.image_memory, 0u);

		for(size_t i= 0u; i < mip_levels.size(); ++i)
		{
			const DDSImage::MipLevel& mip_level= mip_levels[i];

			const GPUDataUploader::RequestResult staging_buffer=
				gpu_data_uploader_.RequestMemory(mip_level.data_size);
			std::memcpy(
				static_cast<char*>(staging_buffer.buffer_data) + staging_buffer.buffer_offset,
				mip_level.data,
				mip_level.data_size);

			const vk::ImageMemoryBarrier image_memory_barrier_transfer(
				vk::AccessFlagBits::eTransferWrite,
				vk::AccessFlagBits::eMemoryRead,
				vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
				queue_family_index_,
				queue_family_index_,
				*out_image.image,
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, uint32_t(i), 1u, 0u, 1u));

			staging_buffer.command_buffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eBottomOfPipe,
				vk::DependencyFlags(),
				0u, nullptr,
				0u, nullptr,
				1u, &image_memory_barrier_transfer);

			const vk::BufferImageCopy copy_region(
				staging_buffer.buffer_offset,
				mip_level.size_rounded[0],
				mip_level.size_rounded[1],
				vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, uint32_t(i), 0u, 1u),
				vk::Offset3D(0, 0, 0),
				vk::Extent3D(mip_level.size[0], mip_level.size[1], 1u));

			staging_buffer.command_buffer.copyBufferToImage(
				staging_buffer.buffer,
				*out_image.image,
				vk::ImageLayout::eTransferDstOptimal,
				1u, &copy_region);

			if(i + 1u == mip_levels.size())
			{
				const vk::ImageMemoryBarrier image_memory_barrier_final(
					vk::AccessFlagBits::eTransferWrite,
					vk::AccessFlagBits::eMemoryRead,
					vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
					queue_family_index_,
					queue_family_index_,
					*out_image.image,
					vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, uint32_t(mip_levels.size()), 0u, 1u));

				staging_buffer.command_buffer.pipelineBarrier(
					vk::PipelineStageFlagBits::eTransfer,
					vk::PipelineStageFlagBits::eBottomOfPipe,
					vk::DependencyFlags(),
					0u, nullptr,
					0u, nullptr,
					1u, &image_memory_barrier_final);
			}
		} // for mip levels

		out_image.image_view= vk_device_.createImageViewUnique(
			vk::ImageViewCreateInfo(
				vk::ImageViewCreateFlags(),
				*out_image.image,
				vk::ImageViewType::e2D,
				image.GetFormat(),
				vk::ComponentMapping(),
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, uint32_t(mip_levels.size()), 0u, 1u)));
	}
	else if(const auto image_loaded_opt= Image::Load(("textures/" + image_name + ".png")))
	{
		const Image& image= *image_loaded_opt;

		const uint32_t mip_levels= uint32_t(std::log2(float(std::min(image.GetWidth(), image.GetHeight()))) - 1.0f);

		out_image.image= vk_device_.createImageUnique(
			vk::ImageCreateInfo(
				vk::ImageCreateFlags(),
				vk::ImageType::e2D,
				vk::Format::eR8G8B8A8Unorm,
				vk::Extent3D(image.GetWidth(), image.GetHeight(), 1u),
				mip_levels,
				1u,
				vk::SampleCountFlagBits::e1,
				vk::ImageTiling::eOptimal,
				vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
				vk::SharingMode::eExclusive,
				0u, nullptr,
				vk::ImageLayout::eUndefined));

		const vk::MemoryRequirements memory_requirements= vk_device_.getImageMemoryRequirements(*out_image.image);

		vk::MemoryAllocateInfo memory_allocate_info(memory_requirements.size);
		for(uint32_t j= 0u; j < memory_properties_.memoryTypeCount; ++j)
		{
			if((memory_requirements.memoryTypeBits & (1u << j)) != 0 &&
				(memory_properties_.memoryTypes[j].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				memory_allocate_info.memoryTypeIndex= j;
		}

		out_image.image_memory= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindImageMemory(*out_image.image, *out_image.image_memory, 0u);

		const GPUDataUploader::RequestResult staging_buffer=
			gpu_data_uploader_.RequestMemory(image.GetWidth() * image.GetHeight() * 4u);
		std::memcpy(
			static_cast<char*>(staging_buffer.buffer_data) + staging_buffer.buffer_offset,
			image.GetData(),
			image.GetWidth() * image.GetHeight() * 4u);

		const vk::ImageMemoryBarrier image_memory_transfer_init(
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eMemoryRead,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
			queue_family_index_,
			queue_family_index_,
			*out_image.image,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u));

		staging_buffer.command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eBottomOfPipe,
			vk::DependencyFlags(),
			0u, nullptr,
			0u, nullptr,
			1u, &image_memory_transfer_init);

		const vk::BufferImageCopy copy_region(
			staging_buffer.buffer_offset,
			image.GetWidth(), image.GetHeight(),
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0u, 0u, 1u),
			vk::Offset3D(0, 0, 0),
			vk::Extent3D(image.GetWidth(), image.GetHeight(), 1u));

		staging_buffer.command_buffer.copyBufferToImage(
			staging_buffer.buffer,
			*out_image.image,
			vk::ImageLayout::eTransferDstOptimal,
			1u, &copy_region);

		const auto image_layout_final= vk::ImageLayout::eShaderReadOnlyOptimal;
		for(uint32_t j= 1u; j < mip_levels; ++j)
		{
			// Transform previous mip level layout from dst_optimal to src_optimal
			const vk::ImageMemoryBarrier vk_image_memory_barrier_src(
				vk::AccessFlagBits::eTransferWrite,
				vk::AccessFlagBits::eMemoryRead,
				vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
				queue_family_index_,
				queue_family_index_,
				*out_image.image,
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, j - 1u, 1u, 0u, 1u));

			staging_buffer.command_buffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eBottomOfPipe,
				vk::DependencyFlags(),
				0u, nullptr,
				0u, nullptr,
				1u, &vk_image_memory_barrier_src);

			const vk::ImageMemoryBarrier vk_image_memory_barrier_dst(
				vk::AccessFlagBits::eTransferWrite,
				vk::AccessFlagBits::eMemoryRead,
				vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
				queue_family_index_,
				queue_family_index_,
				*out_image.image,
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, j, 1u, 0u, 1u));

			staging_buffer.command_buffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eBottomOfPipe,
				vk::DependencyFlags(),
				0u, nullptr,
				0u, nullptr,
				1u, &vk_image_memory_barrier_dst);

			const vk::ImageBlit image_blit(
				vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, j - 1u, 0u, 1u),
				{
					vk::Offset3D(0, 0, 0),
					vk::Offset3D(image.GetWidth () >> (j-1u), image.GetHeight() >> (j-1u), 1),
				},
				vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, j - 0u, 0u, 1u),
				{
					vk::Offset3D(0, 0, 0),
					vk::Offset3D(image.GetWidth () >> j, image.GetHeight() >> j, 1),
				});

			staging_buffer.command_buffer.blitImage(
				*out_image.image,
				vk::ImageLayout::eTransferSrcOptimal,
				*out_image.image,
				vk::ImageLayout::eTransferDstOptimal,
				1u, &image_blit,
				vk::Filter::eLinear);

			const vk::ImageMemoryBarrier vk_image_memory_barrier_src_final(
				vk::AccessFlagBits::eTransferWrite,
				vk::AccessFlagBits::eMemoryRead,
				vk::ImageLayout::eTransferSrcOptimal, image_layout_final,
				queue_family_index_,
				queue_family_index_,
				*out_image.image,
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, j - 1u, 1u, 0u, 1u));

			staging_buffer.command_buffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eBottomOfPipe,
				vk::DependencyFlags(),
				0u, nullptr,
				0u, nullptr,
				1u, &vk_image_memory_barrier_src_final);

			if(j + 1u == mip_levels)
			{
				const vk::ImageMemoryBarrier vk_image_memory_barrier_src_final(
					vk::AccessFlagBits::eTransferWrite,
					vk::AccessFlagBits::eMemoryRead,
					vk::ImageLayout::eTransferDstOptimal, image_layout_final,
					queue_family_index_,
					queue_family_index_,
					*out_image.image,
					vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, j, 1u, 0u, 1u));

				staging_buffer.command_buffer.pipelineBarrier(
					vk::PipelineStageFlagBits::eTransfer,
					vk::PipelineStageFlagBits::eBottomOfPipe,
					vk::DependencyFlags(),
					0u, nullptr,
					0u, nullptr,
					1u, &vk_image_memory_barrier_src_final);
			}
		} // for mips

		out_image.image_view= vk_device_.createImageViewUnique(
			vk::ImageViewCreateInfo(
				vk::ImageViewCreateFlags(),
				*out_image.image,
				vk::ImageViewType::e2D,
				vk::Format::eR8G8B8A8Unorm,
				vk::ComponentMapping(),
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, mip_levels, 0u, 1u)));
	}
	else
		return;

	images_[image_name]= std::move(out_image);
}

const WorldRenderer::ImageGPU& WorldRenderer::GetMaterialAlbedoImage(const Material& material)
{
	const auto it= images_.find(material.albedo_image_id);
	if(it != images_.end())
		return it->second;

	return images_.find(stub_albedo_image_id_)->second;
}

const WorldRenderer::ImageGPU& WorldRenderer::GetMaterialNormalsImage(const Material& material)
{
	const auto it= images_.find(material.normals_image_id);
	if(it != images_.end())
		return it->second;

	return images_.find(stub_normal_map_image_id_)->second;
}

void WorldRenderer::ComandTestLightAdd(const CommandsArguments& args)
{
	if(args.size() < 1u)
	{
		Log::Info("Too few arguments. Usage: test_light_add <radius> <power>");
		return;
	}

	test_light_.emplace();
	test_light_->pos= camera_controller_.GetCameraPosition();
	test_light_->radius= float(std::atof(args[0].c_str()));
	test_light_->color= m_Vec3(1.0f, 0.5f, 1.0f);
	if(args.size() >= 2u)
		test_light_->color*= float(std::atof(args[1].c_str()));
}

void WorldRenderer::CommandTestLightRemove()
{
	test_light_= std::nullopt;
}

} // namespace KK
