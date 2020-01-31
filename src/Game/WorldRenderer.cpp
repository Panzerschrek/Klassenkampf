#include "WorldRenderer.hpp"
#include "Assert.hpp"
#include "Image.hpp"
#include "../Common/MemoryMappedFile.hpp"
#include "../Common/SegmentModelFormat.hpp"
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

struct WorldVertex
{
	float pos[3];
	float tex_coord[2];
	int8_t normal[3];
};

const uint32_t g_tex_uniform_binding= 0u;

const float g_box_vertices[][3]
{
	{ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, // -y
	{ 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f }, // +y
	{ 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f }, // +x
	{ 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, // -x
	{ 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f }, // +z
	{ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, // -z
};

const uint16_t g_box_indices[]
{
	 0,  1,  2,   0,  2,  3,
	 4,  6,  5,   4,  7,  6,
	 8,  9, 10,   8, 10, 11,
	12, 14, 13,  12, 15, 14,
	16, 17, 18,  16, 18, 19,
	20, 22, 21,  20, 23, 22,
};

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
				vk::SamplerMipmapMode::eNearest,
				vk::SamplerAddressMode::eRepeat,
				vk::SamplerAddressMode::eRepeat,
				vk::SamplerAddressMode::eRepeat,
				0.0f,
				VK_FALSE,
				0.0f,
				VK_FALSE,
				vk::CompareOp::eNever,
				0.0f,
				0.0f,
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
		sizeof(m_Mat4));

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

	// Create vertex buffer
	std::vector<WorldVertex> world_vertices;
	std::vector<uint16_t> world_indeces;
	for(const WorldData::Segment& segment : world.segments)
	{
		const size_t index_offset= world_vertices.size();

		const float c_general_scale= 0.25;
		const float s[3]
		{
			c_general_scale * float(segment.bb_max[0] - segment.bb_min[0]),
			c_general_scale * float(segment.bb_max[1] - segment.bb_min[1]),
			c_general_scale * float(segment.bb_max[2] - segment.bb_min[2]),
		};
		const float d[3]
		{
			c_general_scale * float(segment.bb_min[0]),
			c_general_scale * float(segment.bb_min[1]),
			c_general_scale * float(segment.bb_min[2]),
		};
		for(const auto& in_vertex : g_box_vertices)
		{
			const float c_inv_sqr_3= 1.0f / std::sqrt(3.0f);
			WorldVertex out_vertex;
			out_vertex.pos[0]= d[0] + in_vertex[0] * s[0];
			out_vertex.pos[1]= d[1] + in_vertex[1] * s[1];
			out_vertex.pos[2]= d[2] + in_vertex[2] * s[2];
			out_vertex.tex_coord[0]= out_vertex.pos[0] - out_vertex.pos[1];
			out_vertex.tex_coord[1]= out_vertex.pos[0] * c_inv_sqr_3 + out_vertex.pos[1] * c_inv_sqr_3 + out_vertex.pos[2] * c_inv_sqr_3;
			out_vertex.normal[0]= 127;
			out_vertex.normal[1]= 127;
			out_vertex.normal[2]= 127;
			world_vertices.push_back(out_vertex);
		}

		for(const uint16_t index : g_box_indices)
			world_indeces.push_back(uint16_t(index + index_offset));
	}
	index_count_= world_indeces.size();

	{
		vk_vertex_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					world_vertices.size() * sizeof(WorldVertex),
					vk::BufferUsageFlagBits::eVertexBuffer));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vk_vertex_buffer_);

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties_.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		vk_vertex_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*vk_vertex_buffer_, *vk_vertex_buffer_memory_, 0u);

		void* vertex_data_gpu_size= nullptr;
		vk_device_.mapMemory(*vk_vertex_buffer_memory_, 0u, vk_memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &vertex_data_gpu_size);
		std::memcpy(vertex_data_gpu_size, world_vertices.data(), world_vertices.size() * sizeof(WorldVertex));
		vk_device_.unmapMemory(*vk_vertex_buffer_memory_);
	}

	{
		vk_index_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					world_indeces.size() * sizeof(uint16_t),
					vk::BufferUsageFlagBits::eIndexBuffer));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vk_index_buffer_);

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties_.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		vk_index_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*vk_index_buffer_, *vk_index_buffer_memory_, 0u);

		void* vertex_data_gpu_size= nullptr;
		vk_device_.mapMemory(*vk_index_buffer_memory_, 0u, vk_memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &vertex_data_gpu_size);
		std::memcpy(vertex_data_gpu_size, world_indeces.data(), world_indeces.size() * sizeof(uint16_t));
		vk_device_.unmapMemory(*vk_index_buffer_memory_);
	}

	// Create texture.
	{
		const auto image_loaded_opt= Image::Load("test_image.png");
		KK_ASSERT(image_loaded_opt != std::nullopt);
		const Image& image_loaded= *image_loaded_opt;

		vk_image_= vk_device_.createImageUnique(
			vk::ImageCreateInfo(
				vk::ImageCreateFlags(),
				vk::ImageType::e2D,
				vk::Format::eR8G8B8A8Unorm,
				vk::Extent3D(image_loaded.GetWidth(), image_loaded.GetHeight(), 1u),
				1u,
				1u,
				vk::SampleCountFlagBits::e1,
				vk::ImageTiling::eLinear,
				vk::ImageUsageFlagBits::eSampled,
				vk::SharingMode::eExclusive,
				0u, nullptr,
				vk::ImageLayout::ePreinitialized));

		const vk::MemoryRequirements image_memory_requirements= vk_device_.getImageMemoryRequirements(*vk_image_);

		vk::MemoryAllocateInfo vk_memory_allocate_info(image_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties_.memoryTypeCount; ++i)
		{
			if((image_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		vk_image_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindImageMemory(*vk_image_, *vk_image_memory_, 0u);

		const vk::SubresourceLayout image_layout=
			vk_device_.getImageSubresourceLayout(
				*vk_image_,
				vk::ImageSubresource(vk::ImageAspectFlagBits::eColor, 0u, 0u));

		void* texture_data_gpu_size= nullptr;
		vk_device_.mapMemory(*vk_image_memory_, 0u, vk_memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &texture_data_gpu_size);

		for(uint32_t y= 0u; y < image_loaded.GetHeight(); ++y)
			std::memcpy(
				static_cast<char*>(texture_data_gpu_size) + y * image_layout.rowPitch,
				image_loaded.GetData() + y * image_loaded.GetWidth(),
				image_loaded.GetWidth() * sizeof(uint32_t));

		vk_device_.unmapMemory(*vk_image_memory_);

		vk_image_view_= vk_device_.createImageViewUnique(
			vk::ImageViewCreateInfo(
				vk::ImageViewCreateFlags(),
				*vk_image_,
				vk::ImageViewType::e2D,
				vk::Format::eR8G8B8A8Unorm,
				vk::ComponentMapping(vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA),
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u)));

		// Make image layout transition.
		const vk::CommandBuffer command_buffer= window_vulkan.BeginFrame();

		const vk::ImageMemoryBarrier vk_image_memory_barrier(
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eMemoryRead,
			vk::ImageLayout::ePreinitialized, vk::ImageLayout::eGeneral,
			window_vulkan.GetQueueFamilyIndex(),
			window_vulkan.GetQueueFamilyIndex(),
			*vk_image_,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u));

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eBottomOfPipe,
			vk::DependencyFlags(),
			0u, nullptr,
			0u, nullptr,
			1u, &vk_image_memory_barrier);

		window_vulkan.EndFrame({});
	}

	segment_model_= LoadSegmentModel("sponza.kks");

	// Create descriptor set pool.
	const vk::DescriptorPoolSize vk_descriptor_pool_size(
		vk::DescriptorType::eCombinedImageSampler,
		uint32_t(1u + segment_model_.materials.size()));
	vk_descriptor_pool_=
		vk_device_.createDescriptorPoolUnique(
			vk::DescriptorPoolCreateInfo(
				vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
				uint32_t(1u + segment_model_.materials.size()), // max sets.
				1u, &vk_descriptor_pool_size));

	{
		// Create descriptor set.
		vk_descriptor_set_=
			std::move(
			vk_device_.allocateDescriptorSetsUnique(
				vk::DescriptorSetAllocateInfo(
					*vk_descriptor_pool_,
					1u, &*vk_decriptor_set_layout_)).front());

		// Write descriptor set.
		const vk::DescriptorImageInfo descriptor_image_info(
			vk::Sampler(),
			*vk_image_view_,
			vk::ImageLayout::eGeneral);

		const vk::WriteDescriptorSet write_descriptor_set(
			*vk_descriptor_set_,
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

	for(SegmentModel::Material& material : segment_model_.materials)
	{
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
			vk::ImageLayout::eGeneral);

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

void WorldRenderer::BeginFrame(const vk::CommandBuffer command_buffer, const m_Mat4& view_matrix)
{
	tonemapper_.DoRenderPass(
		command_buffer,
		[&]
		{
			command_buffer.pushConstants(
				*vk_pipeline_layout_,
				vk::ShaderStageFlagBits::eVertex,
				0,
				sizeof(view_matrix),
				&view_matrix);

			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *vk_pipeline_);

			if(false)
			{
				command_buffer.bindDescriptorSets(
					vk::PipelineBindPoint::eGraphics,
					*vk_pipeline_layout_,
					0u,
					1u, &*vk_descriptor_set_,
					0u, nullptr);

				const vk::DeviceSize offsets= 0u;
				command_buffer.bindVertexBuffers(0u, 1u, &*vk_vertex_buffer_, &offsets);
				command_buffer.bindIndexBuffer(*vk_index_buffer_, 0u, vk::IndexType::eUint16);

				command_buffer.drawIndexed(uint32_t(index_count_), 1u, 0u, 0u, 0u);
			}
			if(!segment_model_.triangle_groups.empty())
			{
				const vk::DeviceSize offsets= 0u;
				command_buffer.bindVertexBuffers(0u, 1u, &*segment_model_.vertex_buffer, &offsets);
				command_buffer.bindIndexBuffer(*segment_model_.index_buffer, 0u, vk::IndexType::eUint16);

				for(const SegmentModel::TriangleGroup& triangle_group : segment_model_.triangle_groups)
				{
					const SegmentModel::Material& material= segment_model_.materials[triangle_group.material_index];

					const vk::DescriptorSet desctipor_set= material.descriptor_set ? *material.descriptor_set : *vk_descriptor_set_;
					command_buffer.bindDescriptorSets(
						vk::PipelineBindPoint::eGraphics,
						*vk_pipeline_layout_,
						0u,
						1u, &desctipor_set,
						0u, nullptr);

					command_buffer.drawIndexed(triangle_group.index_count, 1u, triangle_group.first_index, triangle_group.first_vertex, 0u);
				}
			}
		});
}

void WorldRenderer::EndFrame(const vk::CommandBuffer command_buffer)
{
	tonemapper_.EndFrame(command_buffer);
}

WorldRenderer::SegmentModel WorldRenderer::LoadSegmentModel(const char* const file_name)
{
	const MemoryMappedFilePtr file_mapped= MemoryMappedFile::Create(file_name);
	KK_ASSERT(file_mapped != nullptr);

	const char* const file_data= static_cast<const char*>(file_mapped->Data());
	const auto& header= *reinterpret_cast<const SegmentModelFormat::SegmentModelHeader*>(file_data);
	const auto* const in_vertices= reinterpret_cast<const SegmentModelFormat::Vertex*>(file_data + header.vertices_offset);
	const auto* const in_indices= reinterpret_cast<const SegmentModelFormat::IndexType*>(file_data + header.indices_offset);
	const auto* const in_triangle_groups= reinterpret_cast<const SegmentModelFormat::TriangleGroup*>(file_data + header.triangle_groups_offset);
	const auto* const in_materials= reinterpret_cast<const SegmentModelFormat::Material*>(file_data + header.materials_offset);

	SegmentModel result;

	result.materials.resize(header.material_count);
	for(size_t i= 0u; i < result.materials.size(); ++i)
	{
		const std::string material_name= static_cast<const char*>(in_materials[i].name);
		const auto image_loaded_opt= Image::Load(("textures/" + material_name + ".png").c_str());
		if(image_loaded_opt == std::nullopt)
			continue;

		const Image& image= *image_loaded_opt;

		const uint32_t mip_levels= 1u;

		SegmentModel::Material& out_material= result.materials[i];
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

		const vk::ImageMemoryBarrier image_memory_transfer_final(
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eMemoryRead,
			vk::ImageLayout::eUndefined, /*vk::ImageLayout::eShaderReadOnlyOptimal*/ vk::ImageLayout::eGeneral,
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
			1u, &image_memory_transfer_final);

		out_material.image_view= vk_device_.createImageViewUnique(
			vk::ImageViewCreateInfo(
				vk::ImageViewCreateFlags(),
				*out_material.image,
				vk::ImageViewType::e2D,
				vk::Format::eR8G8B8A8Unorm,
				vk::ComponentMapping(vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA),
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, mip_levels, 0u, 1u)));
	}

	const float c_scale= 1.0f / 16.0f;
	// Fill vertex buffer.
	// TODO - do not use temporary vector, write to mapped GPU memory instead.
	std::vector<WorldVertex> out_vertices(header.vertex_count);
	for(size_t i= 0u; i < out_vertices.size(); ++i)
	{
		const SegmentModelFormat::Vertex& in_v= in_vertices[i];
		WorldVertex& out_v= out_vertices[i];

		for(size_t j= 0; j < 3u; ++j)
			out_v.pos[j]= c_scale * (header.shift[j] + header.scale[j] * float(in_v.pos[j]));

		std::memcpy(out_v.normal, in_v.normal, 3u);

		for(size_t j= 0u; j < 2u; ++j)
			out_v.tex_coord[j]= float(in_v.tex_coord[j]) / float(SegmentModelFormat::c_tex_coord_scale);
	}

	result.vertex_buffer=
		vk_device_.createBufferUnique(
			vk::BufferCreateInfo(
				vk::BufferCreateFlags(),
				out_vertices.size() * sizeof(WorldVertex),
				vk::BufferUsageFlagBits::eVertexBuffer));

	const vk::MemoryRequirements vertex_buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*result.vertex_buffer);

	vk::MemoryAllocateInfo vertex_buffer_memory_allocate_info(vertex_buffer_memory_requirements.size);
	for(uint32_t i= 0u; i < memory_properties_.memoryTypeCount; ++i)
	{
		if((vertex_buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
			(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
			(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
			vertex_buffer_memory_allocate_info.memoryTypeIndex= i;
	}

	result.vertex_buffer_memory= vk_device_.allocateMemoryUnique(vertex_buffer_memory_allocate_info);
	vk_device_.bindBufferMemory(*result.vertex_buffer, *result.vertex_buffer_memory, 0u);

	void* vertex_data_gpu_size= nullptr;
	vk_device_.mapMemory(*result.vertex_buffer_memory, 0u, vertex_buffer_memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &vertex_data_gpu_size);
	std::memcpy(vertex_data_gpu_size, out_vertices.data(), out_vertices.size() * sizeof(WorldVertex));
	vk_device_.unmapMemory(*result.vertex_buffer_memory);

	// Fill index buffer.
	result.index_buffer=
		vk_device_.createBufferUnique(
			vk::BufferCreateInfo(
				vk::BufferCreateFlags(),
				header.index_count * sizeof(SegmentModelFormat::IndexType),
				vk::BufferUsageFlagBits::eIndexBuffer));

	const vk::MemoryRequirements index_buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*result.index_buffer);
	vk::MemoryAllocateInfo index_buffer_memory_allocate_info(index_buffer_memory_requirements.size);
	for(uint32_t i= 0u; i < memory_properties_.memoryTypeCount; ++i)
	{
		if((index_buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
			(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
			(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
			index_buffer_memory_allocate_info.memoryTypeIndex= i;
	}

	result.index_buffer_memory= vk_device_.allocateMemoryUnique(index_buffer_memory_allocate_info);
	vk_device_.bindBufferMemory(*result.index_buffer, *result.index_buffer_memory, 0u);

	void* index_data_gpu_size= nullptr;
	vk_device_.mapMemory(*result.index_buffer_memory, 0u, index_buffer_memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &index_data_gpu_size);
	std::memcpy(index_data_gpu_size, in_indices, header.index_count * sizeof(SegmentModelFormat::IndexType));
	vk_device_.unmapMemory(*result.index_buffer_memory);

	// Read triangle groups.
	result.triangle_groups.resize(header.triangle_group_count);
	for(size_t i= 0u; i < result.triangle_groups.size(); ++i)
	{
		result.triangle_groups[i].first_vertex= in_triangle_groups[i].first_vertex;
		result.triangle_groups[i].first_index= in_triangle_groups[i].first_index;
		result.triangle_groups[i].index_count= in_triangle_groups[i].index_count;
		result.triangle_groups[i].material_index= in_triangle_groups[i].material_id;
	}

	gpu_data_uploader_.Flush();
	return result;
}

} // namespace KK
