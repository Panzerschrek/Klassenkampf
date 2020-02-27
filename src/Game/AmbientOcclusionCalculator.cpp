#include "AmbientOcclusionCalculator.hpp"
#include "Rand.hpp"
#include "ShaderList.hpp"


namespace KK
{

namespace
{

struct Uniforms
{
	float radius;
	float padding[3];
};

const uint32_t g_tex_uniform_binding= 0u;

} // namespace

AmbientOcclusionCalculator::AmbientOcclusionCalculator(
	Settings& settings,
	WindowVulkan& window_vulkan,
	GPUDataUploader& gpu_data_uploader,
	const Tonemapper& tonemapper)
	: settings_(settings)
	, vk_device_(window_vulkan.GetVulkanDevice())
{
	const auto& memory_properties= window_vulkan.GetMemoryProperties();
	framebuffer_size_= tonemapper.GetFramebufferSize();
	const vk::Format framebuffer_image_format= vk::Format::eR8Unorm;

	{ // Create framebuffer image
		framebuffer_image_=
			vk_device_.createImageUnique(
				vk::ImageCreateInfo(
					vk::ImageCreateFlags(),
					vk::ImageType::e2D,
					framebuffer_image_format,
					vk::Extent3D(framebuffer_size_.width, framebuffer_size_.height, 1u),
					1u,
					1u,
					vk::SampleCountFlagBits::e1,
					vk::ImageTiling::eOptimal,
					vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
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
	}

	framebuffer_image_view_=
		vk_device_.createImageViewUnique(
			vk::ImageViewCreateInfo(
				vk::ImageViewCreateFlags(),
				*framebuffer_image_,
				vk::ImageViewType::e2D,
				framebuffer_image_format,
				vk::ComponentMapping(),
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u)));

	// Create render pass.
	{
		const vk::AttachmentDescription attachment_description(
				vk::AttachmentDescriptionFlags(),
				framebuffer_image_format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eShaderReadOnlyOptimal);

		const vk::AttachmentReference attachment_reference(0u, vk::ImageLayout::eColorAttachmentOptimal);

		const vk::SubpassDescription subpass_description(
				vk::SubpassDescriptionFlags(),
				vk::PipelineBindPoint::eGraphics,
				0u, nullptr,
				1u, &attachment_reference,
				nullptr,
				nullptr);

		render_pass_=
			vk_device_.createRenderPassUnique(
				vk::RenderPassCreateInfo(
					vk::RenderPassCreateFlags(),
					1u, &attachment_description,
					1u, &subpass_description));
	}

	framebuffer_=
		vk_device_.createFramebufferUnique(
			vk::FramebufferCreateInfo(
				vk::FramebufferCreateFlags(),
				*render_pass_,
				1u, &*framebuffer_image_view_,
				framebuffer_size_.width, framebuffer_size_.height, 1u));

	{ // Create random vectors image

		LongRand rand;
		const vk::Extent2D random_vectors_image_size(64, 16);
		const size_t texel_count= random_vectors_image_size.width * random_vectors_image_size.height;
		std::vector<int8_t> texture_data(texel_count * 4u);

		for(size_t i= 0u; i < texel_count; )
		{
			// Generate random vector in cube, skip vectors
			float vec[3];
			for(size_t j= 0u; j < 3u; ++j)
				vec[j]= rand.RandValue(-1.0f, 1.0f);
			const float square_length= vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2];
			if(square_length > 1.0f)
				continue;

			for(size_t j= 0u; j < 3u; ++j)
				texture_data[i * 4u + j]= int8_t(vec[j] * 126.9f);

			++i;
		}

		random_vectors_image_=
			vk_device_.createImageUnique(
				vk::ImageCreateInfo(
					vk::ImageCreateFlags(),
					vk::ImageType::e2D,
					vk::Format::eR8G8B8A8Snorm,
					vk::Extent3D(random_vectors_image_size.width, random_vectors_image_size.height, 1u),
					1u,
					1u,
					vk::SampleCountFlagBits::e1,
					vk::ImageTiling::eOptimal,
					vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
					vk::SharingMode::eExclusive,
					0u, nullptr,
					vk::ImageLayout::eUndefined));

		const vk::MemoryRequirements image_memory_requirements= vk_device_.getImageMemoryRequirements(*random_vectors_image_);

		vk::MemoryAllocateInfo vk_memory_allocate_info(image_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((image_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		random_vectors_image_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindImageMemory(*random_vectors_image_, *random_vectors_image_memory_, 0u);

		const auto staging_buffer= gpu_data_uploader.RequestMemory(texture_data.size() * sizeof(int8_t));

		std::memcpy(
			reinterpret_cast<int8_t*>(staging_buffer.buffer_data) + staging_buffer.buffer_offset,
			texture_data.data(),
			texture_data.size() * sizeof(int8_t));

		const vk::ImageMemoryBarrier image_memory_barrier_transfer(
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eMemoryRead,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
			window_vulkan.GetQueueFamilyIndex(),
			window_vulkan.GetQueueFamilyIndex(),
			*random_vectors_image_,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u));

		staging_buffer.command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eBottomOfPipe,
			vk::DependencyFlags(),
			0u, nullptr,
			0u, nullptr,
			1u, &image_memory_barrier_transfer);

		const vk::BufferImageCopy copy_region(
			staging_buffer.buffer_offset,
			random_vectors_image_size.width ,
			random_vectors_image_size.height,
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0u, 0u, 1u),
			vk::Offset3D(0, 0, 0),
			vk::Extent3D(random_vectors_image_size.width, random_vectors_image_size.height, 1u));

		staging_buffer.command_buffer.copyBufferToImage(
			staging_buffer.buffer,
			*random_vectors_image_,
			vk::ImageLayout::eTransferDstOptimal,
			1u, &copy_region);

		const vk::ImageMemoryBarrier image_memory_barrier_final(
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eMemoryRead,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal,
			window_vulkan.GetQueueFamilyIndex(),
			window_vulkan.GetQueueFamilyIndex(),
			*random_vectors_image_,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u));

		staging_buffer.command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eBottomOfPipe,
			vk::DependencyFlags(),
			0u, nullptr,
			0u, nullptr,
			1u, &image_memory_barrier_final);

		gpu_data_uploader.Flush();
	}

	// Create shaders
	shader_vert_= CreateShader(vk_device_, ShaderNames::ssao_vert);
	shader_frag_= CreateShader(vk_device_, ShaderNames::ssao_frag);

	// Create image sampler
	image_sampler_=
		vk_device_.createSamplerUnique(
			vk::SamplerCreateInfo(
				vk::SamplerCreateFlags(),
				vk::Filter::eNearest,
				vk::Filter::eNearest,
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
	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			g_tex_uniform_binding,
			vk::DescriptorType::eCombinedImageSampler,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			&*image_sampler_,
		},
	};

	descriptor_set_layout_=
		vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eFragment,
		0u,
		sizeof(Uniforms));

	pipeline_layout_=
		vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*descriptor_set_layout_,
				1u, &push_constant_range));

	// Create pipeline.
	const vk::PipelineShaderStageCreateInfo shader_stage_create_info[2]
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

	const vk::PipelineVertexInputStateCreateInfo pipiline_vertex_input_state_create_info(
		vk::PipelineVertexInputStateCreateFlags(),
		0u, nullptr,
		0u, nullptr);

	const vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_create_info(
		vk::PipelineInputAssemblyStateCreateFlags(),
		vk::PrimitiveTopology::eTriangleList);

	const vk::Viewport viewport(0.0f, 0.0f, float(framebuffer_size_.width), float(framebuffer_size_.height), 0.0f, 1.0f);
	const vk::Rect2D scissor(vk::Offset2D(0, 0), framebuffer_size_);

	const vk::PipelineViewportStateCreateInfo pipieline_viewport_state_create_info(
		vk::PipelineViewportStateCreateFlags(),
		1u, &viewport,
		1u, &scissor);

	const vk::PipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info(
		vk::PipelineRasterizationStateCreateFlags(),
		VK_FALSE,
		VK_FALSE,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eNone,
		vk::FrontFace::eCounterClockwise,
		VK_FALSE, 0.0f, 0.0f, 0.0f,
		1.0f);

	const vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info;

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
				uint32_t(std::size(shader_stage_create_info)), shader_stage_create_info,
				&pipiline_vertex_input_state_create_info,
				&pipeline_input_assembly_state_create_info,
				nullptr,
				&pipieline_viewport_state_create_info,
				&pipeline_rasterization_state_create_info,
				&pipeline_multisample_state_create_info,
				nullptr,
				&pipeline_color_blend_state_create_info,
				nullptr,
				*pipeline_layout_,
				*render_pass_,
				0u));

	// Create descriptor set pool.
	const vk::DescriptorPoolSize descriptor_pool_size(vk::DescriptorType::eCombinedImageSampler, 1u);
	descriptor_pool_=
		vk_device_.createDescriptorPoolUnique(
			vk::DescriptorPoolCreateInfo(
				vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
				1u, // max sets.
				1u, &descriptor_pool_size));

	descriptor_set_=
		std::move(
		vk_device_.allocateDescriptorSetsUnique(
			vk::DescriptorSetAllocateInfo(
				*descriptor_pool_,
				1u, &*descriptor_set_layout_)).front());

	// Write descriptor set.
	const vk::DescriptorImageInfo descriptor_image_info(
		vk::Sampler(),
		tonemapper.GetDepthImageView(),
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

AmbientOcclusionCalculator::~AmbientOcclusionCalculator()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

vk::ImageView AmbientOcclusionCalculator::GetAmbientOcclusionImageView() const
{
	return *framebuffer_image_view_;
}

void AmbientOcclusionCalculator::DoPass(const vk::CommandBuffer command_buffer)
{
	command_buffer.beginRenderPass(
		vk::RenderPassBeginInfo(
			*render_pass_,
			*framebuffer_,
			vk::Rect2D(vk::Offset2D(0, 0), framebuffer_size_),
			0u, nullptr),
		vk::SubpassContents::eInline);

	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		*pipeline_layout_,
		0u,
		1u, &*descriptor_set_,
		0u, nullptr);

	Uniforms uniforms;
	command_buffer.pushConstants(
		*pipeline_layout_,
		vk::ShaderStageFlagBits::eFragment,
		0u,
		sizeof(uniforms),
		&uniforms);

	command_buffer.draw(6u, 1u, 0u, 0u);

	command_buffer.endRenderPass();
}

} // namespace KK
