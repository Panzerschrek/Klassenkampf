#include "WorldRenderer.hpp"
#include "../MathLib/MathConstants.hpp"
#include "Assert.hpp"
#include "DDSImage.hpp"
#include "Image.hpp"
#include "Log.hpp"
#include <cmath>
#include <cstring>


namespace KK
{

namespace
{

namespace Shaders
{

#include "shaders/triangle.vert.sprv.h"
#include "shaders/triangle.frag.sprv.h"

} // namespace Shaders

struct Uniforms
{
	m_Mat4 view_matrix;
	m_Mat4 normals_matrix;
};
static_assert(sizeof(Uniforms) <= 128u, "Uniforms size is too big, limit is 128 bytes");

struct WorldVertex
{
	float pos[3];
	float tex_coord[2];
	int8_t normal[3];
};

const uint32_t g_tex_uniform_binding= 0u;

} // namespace

WorldRenderer::WorldRenderer(
	WindowVulkan& window_vulkan,
	GPUDataUploader& gpu_data_uploader,
	const WorldData::World& world)
	: gpu_data_uploader_(gpu_data_uploader)
	, vk_device_(window_vulkan.GetVulkanDevice())
	, viewport_size_(window_vulkan.GetViewportSize())
	, memory_properties_(window_vulkan.GetMemoryProperties())
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, tonemapper_(window_vulkan)
{
	// Create shaders
	shader_vert_=
		vk_device_.createShaderModuleUnique(
			vk::ShaderModuleCreateInfo(
				vk::ShaderModuleCreateFlags(),
				std::size(Shaders::c_triangle_vert_file_content),
				reinterpret_cast<const uint32_t*>(Shaders::c_triangle_vert_file_content)));

	shader_frag_=
		vk_device_.createShaderModuleUnique(
			vk::ShaderModuleCreateInfo(
				vk::ShaderModuleCreateFlags(),
				std::size(Shaders::c_triangle_frag_file_content),
				reinterpret_cast<const uint32_t*>(Shaders::c_triangle_frag_file_content)));

	// Create image sampler
	vk_image_sampler_=
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
				VK_FALSE));

	// Create pipeline layout
	const vk::DescriptorSetLayoutBinding vk_descriptor_set_layout_bindings[]
	{
		{
			g_tex_uniform_binding,
			vk::DescriptorType::eCombinedImageSampler,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			&*vk_image_sampler_,
		},
	};

	vk_decriptor_set_layout_=
		vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(vk_descriptor_set_layout_bindings)), vk_descriptor_set_layout_bindings));

	const vk::PushConstantRange vk_push_constant_range(
		vk::ShaderStageFlagBits::eVertex,
		0u,
		sizeof(Uniforms));

	vk_pipeline_layout_=
		vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u,
				&*vk_decriptor_set_layout_,
				1u,
				&vk_push_constant_range));

	// Create pipeline.
	const vk::PipelineShaderStageCreateInfo vk_shader_stage_create_info[2]
	{
		{
			vk::PipelineShaderStageCreateFlags(),
			vk::ShaderStageFlagBits::eVertex,
			*shader_vert_,
			"main"
		},
		{
			vk::PipelineShaderStageCreateFlags(),
			vk::ShaderStageFlagBits::eFragment,
			*shader_frag_,
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
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

	const vk::PipelineColorBlendStateCreateInfo vk_pipeline_color_blend_state_create_info(
		vk::PipelineColorBlendStateCreateFlags(),
		VK_FALSE,
		vk::LogicOp::eCopy,
		1u, &vk_pipeline_color_blend_attachment_state);

	vk_pipeline_=
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
				*vk_pipeline_layout_,
				tonemapper_.GetRenderPass(),
				0u));

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
	};

	SegmentModels segment_models;
	for(const SegmentModelDescription& segment_model_description : segment_models_names)
	{
		const std::string file_path= "segment_models/" + std::string(segment_model_description.file_name) + ".kks";
		if(std::optional<SegmentModel> model= LoadSegmentModel(file_path))
			segment_models.emplace(segment_model_description.type, std::move(*model));
	}

	// Load materials.
	test_material_id_= "test_image";
	LoadMaterial(test_material_id_);

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
	const vk::DescriptorPoolSize vk_descriptor_pool_size(
		vk::DescriptorType::eCombinedImageSampler,
		uint32_t(materials_.size()));
	vk_descriptor_pool_=
		vk_device_.createDescriptorPoolUnique(
			vk::DescriptorPoolCreateInfo(
				vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
				1u * vk_descriptor_pool_size.descriptorCount, // max sets.
				1u, &vk_descriptor_pool_size));

	for(auto& material_pair : materials_)
	{
		Material& material= material_pair.second;
		if(!material.image_view)
			continue;

		// Create descriptor set.
		material.descriptor_set=
			std::move(
			vk_device_.allocateDescriptorSetsUnique(
				vk::DescriptorSetAllocateInfo(
					*vk_descriptor_pool_,
					1u, &*vk_decriptor_set_layout_)).front());

		// Write descriptor set.
		const vk::DescriptorImageInfo descriptor_image_info(
			vk::Sampler(),
			*material.image_view,
			vk::ImageLayout::eShaderReadOnlyOptimal);

		const vk::WriteDescriptorSet write_descriptor_set(
			*material.descriptor_set,
			g_tex_uniform_binding,
			0u,
			1u,
			vk::DescriptorType::eCombinedImageSampler,
			&descriptor_image_info,
			nullptr,
			nullptr);
		vk_device_.updateDescriptorSets(
			1u, &write_descriptor_set,
			0u, nullptr);
	}

}

WorldRenderer::~WorldRenderer()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

void WorldRenderer::BeginFrame(const vk::CommandBuffer command_buffer, const m_Mat4& view_matrix, const m_Vec3& cam_pos)
{
	tonemapper_.DoRenderPass(
		command_buffer,
		[&]{ DrawWorldModel(command_buffer, world_model_, view_matrix, cam_pos); });
}

void WorldRenderer::EndFrame(const vk::CommandBuffer command_buffer)
{
	tonemapper_.EndFrame(command_buffer);
}

void WorldRenderer::DrawWorldModel(
	const vk::CommandBuffer command_buffer,
	const WorldModel& world_model,
	const m_Mat4& view_matrix,
	const m_Vec3& cam_pos)
{
	const Material& test_material= materials_.find(test_material_id_)->second;

	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *vk_pipeline_);

	Uniforms uniforms;
	uniforms.view_matrix= view_matrix;
	uniforms.normals_matrix.MakeIdentity();

	command_buffer.pushConstants(
		*vk_pipeline_layout_,
		vk::ShaderStageFlagBits::eVertex,
		0,
		sizeof(uniforms),
		&uniforms);

	const vk::DeviceSize offsets= 0u;
	command_buffer.bindVertexBuffers(0u, 1u, &*world_model.vertex_buffer, &offsets);
	command_buffer.bindIndexBuffer(*world_model.index_buffer, 0u, vk::IndexType::eUint16);

	const Sector* cam_sector= nullptr;
	for(const Sector& sector : world_model.sectors)
	{
		if(cam_pos.x >= sector.bb_min.x && cam_pos.x <= sector.bb_max.x &&
			cam_pos.y >= sector.bb_min.y && cam_pos.y <= sector.bb_max.y &&
			cam_pos.z >= sector.bb_min.z && cam_pos.z <= sector.bb_max.z)
		{
			cam_sector= &sector;
			break;
		}
	}

	for(const Sector& sector : world_model.sectors)
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

		for(const Sector::TriangleGroup& triangle_group : sector.triangle_groups)
		{
			const Material& material= materials_.find(triangle_group.material_id)->second;

			const vk::DescriptorSet desctipor_set= material.descriptor_set ? *material.descriptor_set : *test_material.descriptor_set;
			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				*vk_pipeline_layout_,
				0u,
				1u, &desctipor_set,
				0u, nullptr);

			command_buffer.drawIndexed(triangle_group.index_count, 1u, triangle_group.first_index, triangle_group.first_vertex, 0u);
		}
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

			//const size_t segment_first_vertex= world_vertices.size() - size_t(out_sector.first_vertex);
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
					const m_Vec3 normal_transformed= normal * rotate_mat;

					out_v.normal[0]= int8_t(normal_transformed.x);
					out_v.normal[1]= int8_t(normal_transformed.y);
					out_v.normal[2]= int8_t(normal_transformed.z);

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
	const auto* const in_vertices= reinterpret_cast<const SegmentModelFormat::Vertex*>(file_data + header.vertices_offset);
	const auto* const in_indices= reinterpret_cast<const SegmentModelFormat::IndexType*>(file_data + header.indices_offset);
	const auto* const in_triangle_groups= reinterpret_cast<const SegmentModelFormat::TriangleGroup*>(file_data + header.triangle_groups_offset);
	const auto* const in_materials= reinterpret_cast<const SegmentModelFormat::Material*>(file_data + header.materials_offset);

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
			std::move(local_to_global_material_id)
		};
}

void WorldRenderer::LoadMaterial(const std::string& material_name)
{
	if(materials_.count(material_name) > 0)
		return;

	Material& out_material= materials_[material_name];

	if(const auto dds_image_opt= DDSImage::Load(("textures/" + material_name + ".dds")))
	{
		const DDSImage& image= *dds_image_opt;
		const std::vector<DDSImage::MipLevel>& mip_levels= image.GetMipLevels();
		if(mip_levels.empty())
			return;

		KK_ASSERT((mip_levels[0].size[0] & (mip_levels[0].size[0] - 1u)) == 0u);
		KK_ASSERT((mip_levels[0].size[1] & (mip_levels[0].size[1] - 1u)) == 0u);

		out_material.image= vk_device_.createImageUnique(
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

		const vk::MemoryRequirements memory_requirements= vk_device_.getImageMemoryRequirements(*out_material.image);

		vk::MemoryAllocateInfo memory_allocate_info(memory_requirements.size);
		for(uint32_t j= 0u; j < memory_properties_.memoryTypeCount; ++j)
		{
			if((memory_requirements.memoryTypeBits & (1u << j)) != 0 &&
				(memory_properties_.memoryTypes[j].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				memory_allocate_info.memoryTypeIndex= j;
		}

		out_material.image_memory= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindImageMemory(*out_material.image, *out_material.image_memory, 0u);

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
				*out_material.image,
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
				*out_material.image,
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
					*out_material.image,
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

		out_material.image_view= vk_device_.createImageViewUnique(
			vk::ImageViewCreateInfo(
				vk::ImageViewCreateFlags(),
				*out_material.image,
				vk::ImageViewType::e2D,
				image.GetFormat(),
				vk::ComponentMapping(),
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, uint32_t(mip_levels.size()), 0u, 1u)));
	}
	else if(const auto image_loaded_opt= Image::Load(("textures/" + material_name + ".png")))
	{
		const Image& image= *image_loaded_opt;

		const uint32_t mip_levels= uint32_t(std::log2(float(std::min(image.GetWidth(), image.GetHeight()))) - 1.0f);

		out_material.image= vk_device_.createImageUnique(
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

		const vk::MemoryRequirements memory_requirements= vk_device_.getImageMemoryRequirements(*out_material.image);

		vk::MemoryAllocateInfo memory_allocate_info(memory_requirements.size);
		for(uint32_t j= 0u; j < memory_properties_.memoryTypeCount; ++j)
		{
			if((memory_requirements.memoryTypeBits & (1u << j)) != 0 &&
				(memory_properties_.memoryTypes[j].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				memory_allocate_info.memoryTypeIndex= j;
		}

		out_material.image_memory= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindImageMemory(*out_material.image, *out_material.image_memory, 0u);

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
			*out_material.image,
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
			*out_material.image,
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
				*out_material.image,
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
				*out_material.image,
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
				*out_material.image,
				vk::ImageLayout::eTransferSrcOptimal,
				*out_material.image,
				vk::ImageLayout::eTransferDstOptimal,
				1u, &image_blit,
				vk::Filter::eLinear);

			const vk::ImageMemoryBarrier vk_image_memory_barrier_src_final(
				vk::AccessFlagBits::eTransferWrite,
				vk::AccessFlagBits::eMemoryRead,
				vk::ImageLayout::eTransferSrcOptimal, image_layout_final,
				queue_family_index_,
				queue_family_index_,
				*out_material.image,
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
					*out_material.image,
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

		out_material.image_view= vk_device_.createImageViewUnique(
			vk::ImageViewCreateInfo(
				vk::ImageViewCreateFlags(),
				*out_material.image,
				vk::ImageViewType::e2D,
				vk::Format::eR8G8B8A8Unorm,
				vk::ComponentMapping(),
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, mip_levels, 0u, 1u)));
	}
}

} // namespace KK
