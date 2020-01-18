#include "WorldRenderer.hpp"
#include "Assert.hpp"
#include "Image.hpp"
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

#include "shaders/fullscreen_quad.vert.sprv.h"
#include "shaders/tonemapping.frag.sprv.h"

} // namespace Shaders

struct WorldVertex
{
	float pos[3];
	float tex_coord[2];
	uint8_t color[4];
};

const uint32_t g_tex_uniform_binding= 0u;

} // namespace

WorldRenderer::WorldRenderer(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, viewport_size_(window_vulkan.GetViewportSize())
{
	const auto memory_properties= window_vulkan.GetMemoryProperties();
	const vk::Extent2D viewport_size= window_vulkan.GetViewportSize();

	// Create framebuffer with image data.
	{
		const auto framebuffer_image_format= vk::Format::eR8G8B8A8Unorm;
		const auto framebuffer_depth_format= vk::Format::eD16Unorm;
		{
			framebuffer_image_=
				vk_device_.createImageUnique(
					vk::ImageCreateInfo(
						vk::ImageCreateFlags(),
						vk::ImageType::e2D,
						framebuffer_image_format,
						vk::Extent3D(viewport_size.width, viewport_size.height, 1u),
						1u,
						1u,
						vk::SampleCountFlagBits::e1,
						vk::ImageTiling::eLinear,
						vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
						vk::SharingMode::eExclusive,
						0u, nullptr,
						vk::ImageLayout::eUndefined));

			const vk::MemoryRequirements image_memory_requirements= vk_device_.getImageMemoryRequirements(*framebuffer_image_);

			vk::MemoryAllocateInfo vk_memory_allocate_info(image_memory_requirements.size);
			for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
			{
				if((image_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
					(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
					vk_memory_allocate_info.memoryTypeIndex= i;
			}

			framebuffer_image_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
			vk_device_.bindImageMemory(*framebuffer_image_, *framebuffer_image_memory_, 0u);

			framebuffer_image_view_=
				vk_device_.createImageViewUnique(
					vk::ImageViewCreateInfo(
						vk::ImageViewCreateFlags(),
						*framebuffer_image_,
						vk::ImageViewType::e2D,
						framebuffer_image_format,
						vk::ComponentMapping(),
						vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u)));
		}

		{
			framebuffer_depth_image_=
				vk_device_.createImageUnique(
					vk::ImageCreateInfo(
						vk::ImageCreateFlags(),
						vk::ImageType::e2D,
						framebuffer_depth_format,
						vk::Extent3D(viewport_size.width, viewport_size.height, 1u),
						1u,
						1u,
						vk::SampleCountFlagBits::e1,
						vk::ImageTiling::eOptimal,
						vk::ImageUsageFlagBits::eDepthStencilAttachment,
						vk::SharingMode::eExclusive,
						0u, nullptr,
						vk::ImageLayout::eUndefined));

			const vk::MemoryRequirements image_memory_requirements= vk_device_.getImageMemoryRequirements(*framebuffer_depth_image_);

			vk::MemoryAllocateInfo vk_memory_allocate_info(image_memory_requirements.size);
			for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
			{
				if((image_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
					(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
					vk_memory_allocate_info.memoryTypeIndex= i;
			}

			framebuffer_depth_image_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
			vk_device_.bindImageMemory(*framebuffer_depth_image_, *framebuffer_depth_image_memory_, 0u);

			framebuffer_depth_image_view_=
				vk_device_.createImageViewUnique(
					vk::ImageViewCreateInfo(
						vk::ImageViewCreateFlags(),
						*framebuffer_depth_image_,
						vk::ImageViewType::e2D,
						framebuffer_depth_format,
						vk::ComponentMapping(),
						vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0u, 1u, 0u, 1u)));
		}

		const vk::AttachmentDescription vk_attachment_description[]
		{
			{
				vk::AttachmentDescriptionFlags(),
				framebuffer_image_format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eClear,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eGeneral,
			},
			{
				vk::AttachmentDescriptionFlags(),
				framebuffer_depth_format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eClear,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eClear,
				vk::AttachmentStoreOp::eStore,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eTransferSrcOptimal,
			},
		};

		const vk::AttachmentReference vk_attachment_reference_color(0u, vk::ImageLayout::eColorAttachmentOptimal);
		const vk::AttachmentReference vk_attachment_reference_depth(1u, vk::ImageLayout::eGeneral);

		const vk::SubpassDescription vk_subpass_description(
				vk::SubpassDescriptionFlags(),
				vk::PipelineBindPoint::eGraphics,
				0u, nullptr,
				1u, &vk_attachment_reference_color,
				nullptr,
				&vk_attachment_reference_depth);

		framebuffer_render_pass_=
			vk_device_.createRenderPassUnique(
				vk::RenderPassCreateInfo(
					vk::RenderPassCreateFlags(),
					uint32_t(std::size(vk_attachment_description)), vk_attachment_description,
					1u, &vk_subpass_description));

		const vk::ImageView framebuffer_images[]{ *framebuffer_image_view_, *framebuffer_depth_image_view_ };
		framebuffer_=
			vk_device_.createFramebufferUnique(
				vk::FramebufferCreateInfo(
					vk::FramebufferCreateFlags(),
					*framebuffer_render_pass_,
					2u, framebuffer_images,
					viewport_size.width , viewport_size.height, 1u));
	}

	// Create shaders
	{
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
			/*
			{
				0u,
				vk::DescriptorType::eUniformBuffer,
				1u,
				vk::ShaderStageFlagBits::eVertex
			},
			*/
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
			{2u, 0u, vk::Format::eR8G8B8A8Unorm, offsetof(WorldVertex, color)},
		};

		const vk::PipelineVertexInputStateCreateInfo vk_pipiline_vertex_input_state_create_info(
			vk::PipelineVertexInputStateCreateFlags(),
			1u, &vk_vertex_input_binding_description,
			uint32_t(std::size(vk_vertex_input_attribute_description)), vk_vertex_input_attribute_description);

		const vk::PipelineInputAssemblyStateCreateInfo vk_pipeline_input_assembly_state_create_info(
			vk::PipelineInputAssemblyStateCreateFlags(),
			vk::PrimitiveTopology::eTriangleList);

		const vk::Viewport vk_viewport(0.0f, 0.0f, float(viewport_size.width), float(viewport_size.height), 0.0f, 1.0f);
		const vk::Rect2D vk_scissor(vk::Offset2D(0, 0), viewport_size);

		const vk::PipelineViewportStateCreateInfo vk_pipieline_viewport_state_create_info(
			vk::PipelineViewportStateCreateFlags(),
			1u, &vk_viewport,
			1u, &vk_scissor);

		const vk::PipelineRasterizationStateCreateInfo vk_pipilane_rasterization_state_create_info(
			vk::PipelineRasterizationStateCreateFlags(),
			VK_FALSE,
			VK_FALSE,
			vk::PolygonMode::eFill,
			vk::CullModeFlagBits::eNone,
			vk::FrontFace::eCounterClockwise,
			VK_FALSE, 0.0f, 0.0f, 0.0f,
			1.0f);

		const vk::PipelineMultisampleStateCreateInfo vk_pipeline_multisample_state_create_info;

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
					*framebuffer_render_pass_,
					0u));
	}
	// Create vertex buffer

	const std::vector<WorldVertex> world_vertices
	{
		{ { -0.5f, 2.5f, -0.5f }, { 0.0f, 1.0f }, { 255, 128, 128, 0 }, },
		{ { +0.5f, 2.0f, -0.5f }, { 1.0f, 1.0f }, { 128, 255, 128, 0 }, },
		{ { +0.5f, 2.5f, +0.5f }, { 1.0f, 0.0f }, { 128, 128, 255, 0 }, },
		{ { -0.5f, 2.0f, +0.5f }, { 0.0f, 0.0f }, { 128, 128, 128, 0 }, },
	};

	const std::vector<uint16_t> world_indeces{ 0, 1, 2, 0, 2, 3 };

	{
		vk_vertex_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					world_vertices.size() * sizeof(WorldVertex),
					vk::BufferUsageFlagBits::eVertexBuffer));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vk_vertex_buffer_);

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
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
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
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
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((image_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
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

	{
		// Create descriptor set pool.
		const vk::DescriptorPoolSize vk_descriptor_pool_size(vk::DescriptorType::eCombinedImageSampler, 1u);
		vk_descriptor_pool_=
			vk_device_.createDescriptorPoolUnique(
				vk::DescriptorPoolCreateInfo(
					vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
					4u, // max sets.
					1u, &vk_descriptor_pool_size));

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

	{
		// Create shaders
		tonemapping_shader_vert_=
			vk_device_.createShaderModuleUnique(
				vk::ShaderModuleCreateInfo(
					vk::ShaderModuleCreateFlags(),
					std::size(Shaders::c_fullscreen_quad_vert_file_content),
					reinterpret_cast<const uint32_t*>(Shaders::c_fullscreen_quad_vert_file_content)));

		tonemapping_shader_frag_=
			vk_device_.createShaderModuleUnique(
				vk::ShaderModuleCreateInfo(
					vk::ShaderModuleCreateFlags(),
					std::size(Shaders::c_tonemapping_frag_file_content),
					reinterpret_cast<const uint32_t*>(Shaders::c_tonemapping_frag_file_content)));

		// Create image sampler

		vk_image_sampler_=
			vk_device_.createSamplerUnique(
				vk::SamplerCreateInfo(
					vk::SamplerCreateFlags(),
					vk::Filter::eNearest,
					vk::Filter::eNearest,
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

		tonemapping_decriptor_set_layout_=
			vk_device_.createDescriptorSetLayoutUnique(
				vk::DescriptorSetLayoutCreateInfo(
					vk::DescriptorSetLayoutCreateFlags(),
					uint32_t(std::size(vk_descriptor_set_layout_bindings)), vk_descriptor_set_layout_bindings));


		tonemapping_pipeline_layout_=
			vk_device_.createPipelineLayoutUnique(
				vk::PipelineLayoutCreateInfo(
					vk::PipelineLayoutCreateFlags(),
					1u, &*vk_decriptor_set_layout_,
					0u, nullptr));

		// Create pipeline.
		const vk::PipelineShaderStageCreateInfo vk_shader_stage_create_info[2]
		{
			{
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eVertex,
				*tonemapping_shader_vert_,
				"main"
			},
			{
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eFragment,
				*tonemapping_shader_frag_,
				"main"
			},
		};

		const vk::PipelineVertexInputStateCreateInfo vk_pipiline_vertex_input_state_create_info(
			vk::PipelineVertexInputStateCreateFlags(),
			0u, nullptr,
			0u, nullptr);

		const vk::PipelineInputAssemblyStateCreateInfo vk_pipeline_input_assembly_state_create_info(
			vk::PipelineInputAssemblyStateCreateFlags(),
			vk::PrimitiveTopology::eTriangleList);

		const vk::Viewport vk_viewport(0.0f, 0.0f, float(viewport_size.width), float(viewport_size.height), 0.0f, 1.0f);
		const vk::Rect2D vk_scissor(vk::Offset2D(0, 0), viewport_size);

		const vk::PipelineViewportStateCreateInfo vk_pipieline_viewport_state_create_info(
			vk::PipelineViewportStateCreateFlags(),
			1u, &vk_viewport,
			1u, &vk_scissor);

		const vk::PipelineRasterizationStateCreateInfo vk_pipilane_rasterization_state_create_info(
			vk::PipelineRasterizationStateCreateFlags(),
			VK_FALSE,
			VK_FALSE,
			vk::PolygonMode::eFill,
			vk::CullModeFlagBits::eNone,
			vk::FrontFace::eCounterClockwise,
			VK_FALSE, 0.0f, 0.0f, 0.0f,
			1.0f);

		const vk::PipelineMultisampleStateCreateInfo vk_pipeline_multisample_state_create_info;

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

		tonemapping_pipeline_=
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
					nullptr,
					&vk_pipeline_color_blend_state_create_info,
					nullptr,
					*tonemapping_pipeline_layout_,
					window_vulkan.GetRenderPass(),
					0u));
	}

	{
		// Create descriptor set pool.
		const vk::DescriptorPoolSize vk_descriptor_pool_size(vk::DescriptorType::eCombinedImageSampler, 1u);
		tonemapping_descriptor_pool_=
			vk_device_.createDescriptorPoolUnique(
				vk::DescriptorPoolCreateInfo(
					vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
					1u, // max sets.
					1u, &vk_descriptor_pool_size));

		// Create descriptor set.
		tonemapping_descriptor_set_=
			std::move(
			vk_device_.allocateDescriptorSetsUnique(
				vk::DescriptorSetAllocateInfo(
					*tonemapping_descriptor_pool_,
					1u, &*tonemapping_decriptor_set_layout_)).front());

		// Write descriptor set.
		const vk::DescriptorImageInfo descriptor_image_info(
			vk::Sampler(),
			*framebuffer_image_view_,
			vk::ImageLayout::eGeneral);

		const vk::WriteDescriptorSet write_descriptor_set(
			*tonemapping_descriptor_set_,
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
	const vk::ClearValue clear_value[]
	{
		{ vk::ClearColorValue(std::array<float,4>{0.2f, 0.1f, 0.1f, 0.5f}) },
		{ vk::ClearDepthStencilValue(1.0f, 0u) },
	};

	command_buffer.beginRenderPass(
		vk::RenderPassBeginInfo(
			*framebuffer_render_pass_,
			*framebuffer_,
			vk::Rect2D(vk::Offset2D(0, 0), viewport_size_),
			2u, clear_value),
		vk::SubpassContents::eInline);

	const vk::DeviceSize offsets= 0u;
	command_buffer.bindVertexBuffers(0u, 1u, &*vk_vertex_buffer_, &offsets);
	command_buffer.bindIndexBuffer(*vk_index_buffer_, 0u, vk::IndexType::eUint16);
	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		*vk_pipeline_layout_,
		0u,
		1u, &*vk_descriptor_set_,
		0u, nullptr);

	command_buffer.pushConstants(*vk_pipeline_layout_, vk::ShaderStageFlagBits::eVertex, 0, sizeof(view_matrix), &view_matrix);

	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *vk_pipeline_);
	command_buffer.drawIndexed(6u, 1u, 0u, 0u, 0u);

	command_buffer.endRenderPass();
}

void WorldRenderer::EndFrame(const vk::CommandBuffer command_buffer)
{
	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *tonemapping_pipeline_);
	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		*tonemapping_pipeline_layout_,
		0u,
		1u, &*tonemapping_descriptor_set_,
		0u, nullptr);

	command_buffer.draw(6u, 1u, 0u, 0u);
}

} // namespace KK
