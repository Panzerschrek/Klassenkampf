#include "Shadowmapper.hpp"
#include "Log.hpp"
#include "../MathLib/Mat.hpp"


namespace KK
{

namespace
{

namespace Shaders
{

#include "shaders/shadow.vert.sprv.h"

} // namespace Shaders

struct Uniforms
{
	m_Mat4 mat;
};

} // namespace

Shadowmapper::Shadowmapper(
	WindowVulkan& window_vulkan,
	const size_t vertex_size,
	const size_t vertex_pos_offset,
	const vk::Format vertex_pos_format)
	: vk_device_(window_vulkan.GetVulkanDevice())
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

	shadowmap_size_.width = 4096u;
	shadowmap_size_.height= 4096u;

	Log::Info("Shadowmap size: ", shadowmap_size_.width, "x", shadowmap_size_.height);
	Log::Info("Shadowmap format: ", vk::to_string(depth_format));

	{ // Create depth image.
		depth_image_=
			vk_device_.createImageUnique(
				vk::ImageCreateInfo(
					vk::ImageCreateFlags(),
					vk::ImageType::e2D,
					depth_format,
					vk::Extent3D(shadowmap_size_.width, shadowmap_size_.height, 1u),
					1u,
					1u,
					vk::SampleCountFlagBits::e1,
					vk::ImageTiling::eOptimal,
					vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eSampled,
					vk::SharingMode::eExclusive,
					0u, nullptr,
					vk::ImageLayout::eUndefined));

		const vk::MemoryRequirements image_memory_requirements= vk_device_.getImageMemoryRequirements(*depth_image_);

		vk::MemoryAllocateInfo vk_memory_allocate_info(image_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((image_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		depth_image_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindImageMemory(*depth_image_, *depth_image_memory_, 0u);

		depth_image_view_=
			vk_device_.createImageViewUnique(
				vk::ImageViewCreateInfo(
					vk::ImageViewCreateFlags(),
					*depth_image_,
					vk::ImageViewType::e2D,
					depth_format,
					vk::ComponentMapping(),
					vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0u, 1u, 0u, 1u)));
	}
	{ // Create render pass and framebuffer.
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

		framebuffer_=
			vk_device_.createFramebufferUnique(
				vk::FramebufferCreateInfo(
					vk::FramebufferCreateFlags(),
					*render_pass_,
					1u, &*depth_image_view_,
					shadowmap_size_.width , shadowmap_size_.height, 1u));
	}

	// Create shaders.
	shader_vert_=
		vk_device_.createShaderModuleUnique(
			vk::ShaderModuleCreateInfo(
				vk::ShaderModuleCreateFlags(),
				std::size(Shaders::c_shadow_vert_file_content),
				reinterpret_cast<const uint32_t*>(Shaders::c_shadow_vert_file_content)));

	// Create pipeline layout.
	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eVertex,
		0u,
		sizeof(Uniforms));

	pipeline_layout_=
		vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				0u, nullptr,
				1u, &push_constant_range));

	// Create pipeline.
	const vk::PipelineShaderStageCreateInfo shader_stage_create_info[]
	{
		{
			vk::PipelineShaderStageCreateFlags(),
			vk::ShaderStageFlagBits::eVertex,
			*shader_vert_,
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

	const vk::Viewport viewport(0.0f, 0.0f, float(shadowmap_size_.width), float(shadowmap_size_.height), 0.0f, 1.0f);
	const vk::Rect2D scissor(vk::Offset2D(0, 0), shadowmap_size_);

	const vk::PipelineViewportStateCreateInfo pipieline_viewport_state_create_info(
		vk::PipelineViewportStateCreateFlags(),
		1u, &viewport,
		1u, &scissor);

	const vk::PipelineRasterizationStateCreateInfo pipilane_rasterization_state_create_info(
		vk::PipelineRasterizationStateCreateFlags(),
		VK_FALSE,
		VK_FALSE,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eNone,
		vk::FrontFace::eCounterClockwise,
		VK_TRUE, // Depth bias enabled
		0.0f, // Depth bias constant factor
		0.0f, // Depth bias clamp
		4.0f, // Depth bias slope factor
		1.0f);

	const vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info;

	const vk::PipelineDepthStencilStateCreateInfo pipeline_depth_state_create_info(
		vk::PipelineDepthStencilStateCreateFlags(),
		VK_TRUE,
		VK_TRUE,
		vk::CompareOp::eLess,
		VK_FALSE,
		VK_FALSE,
		vk::StencilOpState(),
		vk::StencilOpState(),
		-1.0f,
		+1.0f);

	const vk::PipelineColorBlendAttachmentState pipeline_color_blend_attachment_state(
		VK_FALSE,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

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
				uint32_t(std::size(shader_stage_create_info)),
				shader_stage_create_info,
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

Shadowmapper::~Shadowmapper()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

void Shadowmapper::DoRenderPass(
	const vk::CommandBuffer command_buffer,
	const std::function<void()>& draw_function)
{
	const vk::ClearValue clear_value(vk::ClearDepthStencilValue(1.0f, 0u));

	command_buffer.beginRenderPass(
		vk::RenderPassBeginInfo(
			*render_pass_,
			*framebuffer_,
			vk::Rect2D(vk::Offset2D(0, 0), shadowmap_size_),
			1u, &clear_value),
		vk::SubpassContents::eInline);

	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);

	draw_function();

	command_buffer.endRenderPass();
}

vk::PipelineLayout Shadowmapper::GetPipelineLayout()
{
	return *pipeline_layout_;
}

vk::ImageView Shadowmapper::GetShadowmapImageView()
{
	return *depth_image_view_;
}

} // namespace KK
