#include "Shadowmapper.hpp"
#include "../MathLib/Mat.hpp"


namespace KK
{

namespace
{

namespace Shaders
{

#include "shaders/cubemap_shadow.vert.sprv.h"
#include "shaders/cubemap_shadow.geom.sprv.h"
#include "shaders/cubemap_shadow.frag.sprv.h"

} // namespace

struct Uniforms
{
	m_Mat4 view_matrices[6];
	m_Vec3 light_pos;
};

} // namespace

Shadowmapper::Shadowmapper(
	WindowVulkan& window_vulkan,
	const size_t vertex_size,
	const size_t vertex_pos_offset,
	const vk::Format vertex_pos_format)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, cubemap_size_(256u)
	, cubemap_count_(64u)
{
	const vk::PhysicalDeviceMemoryProperties& memory_properties= window_vulkan.GetMemoryProperties();

	// Select depth buffer format.
	const vk::Format depth_formats[]
	{
		// Depth formats by priority.
		vk::Format::eD16Unorm,
		vk::Format::eD16UnormS8Uint,
		vk::Format::eD24UnormS8Uint,
		vk::Format::eX8D24UnormPack32,
		vk::Format::eD32Sfloat,
		vk::Format::eD32SfloatS8Uint,
	};
	vk::Format depth_format= vk::Format::eD16Unorm;
	for(const vk::Format depth_format_candidate : depth_formats)
	{
		const vk::FormatProperties format_properties=
			window_vulkan.GetPhysicalDevice().getFormatProperties(depth_format_candidate);

		const vk::FormatFeatureFlags required_falgs= vk::FormatFeatureFlagBits::eDepthStencilAttachment;
		if((format_properties.optimalTilingFeatures & required_falgs) == required_falgs)
		{
			depth_format= depth_format_candidate;
			break;
		}
	}

	{ // Create depth cubemap array image.
		depth_cubemap_array_image_=
			vk_device_.createImageUnique(
				vk::ImageCreateInfo(
					vk::ImageCreateFlagBits::eCubeCompatible,
					vk::ImageType::e2D,
					depth_format,
					vk::Extent3D(cubemap_size_, cubemap_size_, 1u),
					1u,
					cubemap_count_ * 6u,
					vk::SampleCountFlagBits::e1,
					vk::ImageTiling::eOptimal,
					vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
					vk::SharingMode::eExclusive,
					0u, nullptr,
					vk::ImageLayout::eUndefined));

		const vk::MemoryRequirements image_memory_requirements= vk_device_.getImageMemoryRequirements(*depth_cubemap_array_image_);

		vk::MemoryAllocateInfo vk_memory_allocate_info(image_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((image_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		depth_cubemap_array_image_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindImageMemory(*depth_cubemap_array_image_, *depth_cubemap_array_image_memory_, 0u);

		depth_cubemap_array_image_view_=
			vk_device_.createImageViewUnique(
				vk::ImageViewCreateInfo(
					vk::ImageViewCreateFlags(),
					*depth_cubemap_array_image_,
					vk::ImageViewType::eCubeArray,
					depth_format,
					vk::ComponentMapping(),
					vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0u, 1u, 0u, cubemap_count_ * 6u)));
	}

	{ // Create render pass.
		const vk::AttachmentDescription attachment_description(
			vk::AttachmentDescriptionFlags(),
			depth_format,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eShaderReadOnlyOptimal);

		const vk::AttachmentReference attachment_reference(0u, vk::ImageLayout::eDepthStencilAttachmentOptimal);

		const vk::SubpassDescription subpass_description(
			vk::SubpassDescriptionFlags(),
			vk::PipelineBindPoint::eGraphics,
			0u, nullptr,
			0u, nullptr,
			nullptr,
			&attachment_reference);

		render_pass_=
			vk_device_.createRenderPassUnique(
				vk::RenderPassCreateInfo(
					vk::RenderPassCreateFlags(),
					1u, &attachment_description,
					1u, &subpass_description));
	}

	// Create framebuffer for each cubemap.
	for(uint32_t i= 0u; i < cubemap_count_; ++i)
	{
		Framebuffer framebuffer;

		framebuffer.image_view=
			vk_device_.createImageViewUnique(
				vk::ImageViewCreateInfo(
					vk::ImageViewCreateFlags(),
					*depth_cubemap_array_image_,
					vk::ImageViewType::eCube,
					depth_format,
					vk::ComponentMapping(),
					vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0u, 1u, i * 6u, 6u)));

		framebuffer.framebuffer=
			vk_device_.createFramebufferUnique(
				vk::FramebufferCreateInfo(
					vk::FramebufferCreateFlags(),
					*render_pass_,
					1u, &*framebuffer.image_view,
					cubemap_size_, cubemap_size_, 6u));

		framebuffers_.push_back(std::move(framebuffer));
	}

	// Create shaders
	shader_vert_=
		vk_device_.createShaderModuleUnique(
			vk::ShaderModuleCreateInfo(
				vk::ShaderModuleCreateFlags(),
				std::size(Shaders::c_cubemap_shadow_vert_file_content),
				reinterpret_cast<const uint32_t*>(Shaders::c_cubemap_shadow_vert_file_content)));

	shader_geom_=
		vk_device_.createShaderModuleUnique(
			vk::ShaderModuleCreateInfo(
				vk::ShaderModuleCreateFlags(),
				std::size(Shaders::c_cubemap_shadow_geom_file_content),
				reinterpret_cast<const uint32_t*>(Shaders::c_cubemap_shadow_geom_file_content)));

	shader_frag_=
		vk_device_.createShaderModuleUnique(
			vk::ShaderModuleCreateInfo(
				vk::ShaderModuleCreateFlags(),
				std::size(Shaders::c_cubemap_shadow_frag_file_content),
				reinterpret_cast<const uint32_t*>(Shaders::c_cubemap_shadow_frag_file_content)));

	// Create pipeline
	{
		const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
		{
			{
				0u,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eGeometry,
				nullptr,
			},
		};

		descriptor_set_layout_=
			vk_device_.createDescriptorSetLayoutUnique(
				vk::DescriptorSetLayoutCreateInfo(
					vk::DescriptorSetLayoutCreateFlags(),
					uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

		pipeline_layout_=
			vk_device_.createPipelineLayoutUnique(
				vk::PipelineLayoutCreateInfo(
					vk::PipelineLayoutCreateFlags(),
					1u, &*descriptor_set_layout_,
					0u, nullptr));

		const vk::PipelineShaderStageCreateInfo vk_shader_stage_create_info[]
		{
			{
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eVertex,
				*shader_vert_,
				"main"
			},
			{
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eGeometry,
				*shader_geom_,
				"main"
			},
			{
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eFragment,
				*shader_frag_,
				"main"
			},
		};

		const vk::VertexInputBindingDescription vertex_input_binding_description(
			0u, uint32_t(vertex_size), vk::VertexInputRate::eVertex);

		const vk::VertexInputAttributeDescription vertex_input_attribute_description[]
		{
			{0u, 0u, vertex_pos_format, uint32_t(vertex_pos_offset)},
		};

		const vk::PipelineVertexInputStateCreateInfo pipiline_vertex_input_state_create_info(
			vk::PipelineVertexInputStateCreateFlags(),
			1u, &vertex_input_binding_description,
			uint32_t(std::size(vertex_input_attribute_description)), vertex_input_attribute_description);

		const vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_create_info(
			vk::PipelineInputAssemblyStateCreateFlags(),
			vk::PrimitiveTopology::eTriangleList);

		const vk::Viewport viewport(0.0f, 0.0f, float(cubemap_size_), float(cubemap_size_), 0.0f, 1.0f);
		const vk::Rect2D scissor(vk::Offset2D(0, 0), vk::Extent2D(cubemap_size_, cubemap_size_));

		const vk::PipelineViewportStateCreateInfo pipieline_viewport_state_create_info(
			vk::PipelineViewportStateCreateFlags(),
			1u, &viewport,
			1u, &scissor);

		const vk::PipelineRasterizationStateCreateInfo pipilane_rasterization_state_create_info(
			vk::PipelineRasterizationStateCreateFlags(),
			VK_FALSE,
			VK_FALSE,
			vk::PolygonMode::eFill,
			vk::CullModeFlagBits::eBack,
			vk::FrontFace::eCounterClockwise,
			VK_FALSE, 0.0f, 0.0f, 0.0f,
			1.0f);

		const vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info(
			vk::PipelineMultisampleStateCreateFlags(),
			vk::SampleCountFlagBits::e1);

		const vk::PipelineDepthStencilStateCreateInfo pipeline_depth_state_create_info(
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

		const vk::PipelineColorBlendAttachmentState pipeline_color_blend_attachment_state;

		const vk::PipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info(
			vk::PipelineColorBlendStateCreateFlags(),
			VK_FALSE,
			vk::LogicOp::eCopy,
			1u, &pipeline_color_blend_attachment_state);

		pipeline_=
			vk_device_.createGraphicsPipelineUnique(
				nullptr,
				vk::GraphicsPipelineCreateInfo(
					vk::PipelineCreateFlags(),
					uint32_t(std::size(vk_shader_stage_create_info)),
					vk_shader_stage_create_info,
					&pipiline_vertex_input_state_create_info,
					&pipeline_input_assembly_state_create_info,
					nullptr,
					&pipieline_viewport_state_create_info,
					&pipilane_rasterization_state_create_info,
					&pipeline_multisample_state_create_info,
					&pipeline_depth_state_create_info,
					&pipeline_color_blend_state_create_info,
					nullptr,
					*pipeline_layout_,
					*render_pass_,
					0u));
	}

	// Create uniforms buffer.
	{
		uniforms_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					sizeof(Uniforms),
					vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*uniforms_buffer_);

		vk::MemoryAllocateInfo memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				memory_allocate_info.memoryTypeIndex= i;
		}

		uniforms_buffer_memory_= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindBufferMemory(*uniforms_buffer_, *uniforms_buffer_memory_, 0u);
	}

	// Create descriptor set pool.
	const vk::DescriptorPoolSize descriptor_pool_size(vk::DescriptorType::eStorageBuffer, 1u);
	descriptor_set_pool_=
		vk_device_.createDescriptorPoolUnique(
			vk::DescriptorPoolCreateInfo(
				vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
				1u, // max sets.
				1u, &descriptor_pool_size));

	// Create descriptor set.
	descriptor_set_=
		std::move(
		vk_device_.allocateDescriptorSetsUnique(
			vk::DescriptorSetAllocateInfo(
				*descriptor_set_pool_,
				1u, &*descriptor_set_layout_)).front());

	// Write descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_buffer_info(
			*uniforms_buffer_,
			0u,
			sizeof(Uniforms));

		const vk::WriteDescriptorSet write_descriptor_set(
			*descriptor_set_,
			0u,
			0u,
			1u,
			vk::DescriptorType::eStorageBuffer,
			nullptr,
			&descriptor_buffer_info,
			nullptr);
		vk_device_.updateDescriptorSets(
			1u, &write_descriptor_set,
			0u, nullptr);
	}
}

Shadowmapper::~Shadowmapper()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

vk::ImageView Shadowmapper::GetDepthCubemapArrayImageView() const
{
	return *depth_cubemap_array_image_view_;
}

} // namespace KK
