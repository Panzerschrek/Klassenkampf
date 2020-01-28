#include "Tonemapper.hpp"


namespace KK
{


namespace
{

namespace Shaders
{

#include "shaders/fullscreen_quad.vert.sprv.h"
#include "shaders/tonemapping.frag.sprv.h"

} // namespace Shaders

const uint32_t g_tex_uniform_binding= 0u;

} // namespace

Tonemapper::Tonemapper(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, msaa_sample_count_(vk::SampleCountFlagBits::e1)
{
	const vk::Extent2D viewport_size= window_vulkan.GetViewportSize();
	const vk::PhysicalDeviceMemoryProperties& memory_properties= window_vulkan.GetMemoryProperties();
	const auto framebuffer_image_format= vk::Format::eR8G8B8A8Unorm;

	// Select depth buffer format.
	const vk::Format depth_formats[]
	{
	// Depth formats by priority.
		vk::Format::eD32Sfloat,
		vk::Format::eD24UnormS8Uint,
		vk::Format::eX8D24UnormPack32,
		vk::Format::eD32SfloatS8Uint,
		vk::Format::eD16Unorm,
		vk::Format::eD16UnormS8Uint,
	};
	vk::Format framebuffer_depth_format= vk::Format::eD16Unorm;
	for(const vk::Format depth_format_candidate : depth_formats)
	{
		const vk::FormatProperties format_properties=
			window_vulkan.GetPhysicalDevice().getFormatProperties(depth_format_candidate);

		const vk::FormatFeatureFlags required_falgs= vk::FormatFeatureFlagBits::eDepthStencilAttachment;
		if((format_properties.optimalTilingFeatures & required_falgs) == required_falgs)
		{
			framebuffer_depth_format= depth_format_candidate;
			break;
		}
	}

	// Use bigger framebuffer, because we use lenses distortion effect.
	framebuffer_size_.width = viewport_size.width  * 4u / 3u;
	framebuffer_size_.height= viewport_size.height * 4u / 3u;
	framebuffer_size_.width = (framebuffer_size_.width  + 7u) & ~7u;
	framebuffer_size_.height= (framebuffer_size_.height + 7u) & ~7u;

	{
		framebuffer_image_=
			vk_device_.createImageUnique(
				vk::ImageCreateInfo(
					vk::ImageCreateFlags(),
					vk::ImageType::e2D,
					framebuffer_image_format,
					vk::Extent3D(framebuffer_size_.width, framebuffer_size_.height, 1u),
					1u,
					1u,
					msaa_sample_count_,
					vk::ImageTiling::eOptimal,
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
					vk::Extent3D(framebuffer_size_.width, framebuffer_size_.height, 1u),
					1u,
					1u,
					msaa_sample_count_,
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
			msaa_sample_count_,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eShaderReadOnlyOptimal,
		},
		{
			vk::AttachmentDescriptionFlags(),
			framebuffer_depth_format,
			msaa_sample_count_,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eGeneral,
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
				framebuffer_size_.width , framebuffer_size_.height, 1u));

	// Create shaders
	shader_vert_=
		vk_device_.createShaderModuleUnique(
			vk::ShaderModuleCreateInfo(
				vk::ShaderModuleCreateFlags(),
				std::size(Shaders::c_fullscreen_quad_vert_file_content),
				reinterpret_cast<const uint32_t*>(Shaders::c_fullscreen_quad_vert_file_content)));

	shader_frag_=
		vk_device_.createShaderModuleUnique(
			vk::ShaderModuleCreateInfo(
				vk::ShaderModuleCreateFlags(),
				std::size(Shaders::c_tonemapping_frag_file_content),
				reinterpret_cast<const uint32_t*>(Shaders::c_tonemapping_frag_file_content)));

	// Create image sampler
	framebuffer_image_sampler_=
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
				VK_FALSE));

	// Create pipeline layout
	const vk::DescriptorSetLayoutBinding vk_descriptor_set_layout_bindings[]
	{
		{
			g_tex_uniform_binding,
			vk::DescriptorType::eCombinedImageSampler,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			&*framebuffer_image_sampler_,
		},
	};

	decriptor_set_layout_=
		vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(vk_descriptor_set_layout_bindings)), vk_descriptor_set_layout_bindings));

	pipeline_layout_=
		vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*decriptor_set_layout_,
				0u, nullptr));

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

	pipeline_=
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
				*pipeline_layout_,
				window_vulkan.GetRenderPass(),
				0u));

	// Create descriptor set pool.
	const vk::DescriptorPoolSize vk_descriptor_pool_size(vk::DescriptorType::eCombinedImageSampler, 1u);
	descriptor_pool_=
		vk_device_.createDescriptorPoolUnique(
			vk::DescriptorPoolCreateInfo(
				vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
				1u, // max sets.
				1u, &vk_descriptor_pool_size));

	// Create descriptor set.
	descriptor_set_=
		std::move(
		vk_device_.allocateDescriptorSetsUnique(
			vk::DescriptorSetAllocateInfo(
				*descriptor_pool_,
				1u, &*decriptor_set_layout_)).front());

	// Write descriptor set.
	const vk::DescriptorImageInfo descriptor_image_info(
		vk::Sampler(),
		*framebuffer_image_view_,
		vk::ImageLayout::eShaderReadOnlyOptimal);

	const vk::WriteDescriptorSet write_descriptor_set(
		*descriptor_set_,
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

vk::Extent2D Tonemapper::GetFramebufferSize() const
{
	return framebuffer_size_;
}

vk::RenderPass Tonemapper::GetRenderPass() const
{
	return *framebuffer_render_pass_;
}

vk::SampleCountFlagBits Tonemapper::GetSampleCount() const
{
	return msaa_sample_count_;
}

void Tonemapper::DoRenderPass(const vk::CommandBuffer command_buffer, const std::function<void()>& draw_function)
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
			vk::Rect2D(vk::Offset2D(0, 0), framebuffer_size_),
			2u, clear_value),
		vk::SubpassContents::eInline);

	draw_function();

	command_buffer.endRenderPass();
}

void Tonemapper::EndFrame(const vk::CommandBuffer command_buffer)
{
	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		*pipeline_layout_,
		0u,
		1u, &*descriptor_set_,
		0u, nullptr);

	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
	command_buffer.draw(6u, 1u, 0u, 0u);
}

} // namespace KK
