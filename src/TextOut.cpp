#include "Assert.hpp"
#include "Image.hpp"
#include "TextOut.hpp"


namespace KK
{

namespace
{

namespace Shaders
{

#include "shaders/text.vert.sprv.h"
#include "shaders/text.frag.sprv.h"

} // namespace Shaders

} // namespace

TextOut::TextOut(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, viewport_size_(window_vulkan.GetViewportSize())
	, render_pass_(window_vulkan.GetRenderPass())
{
	// Create shaders
	shader_vert_=
		vk_device_.createShaderModuleUnique(
			vk::ShaderModuleCreateInfo(
				vk::ShaderModuleCreateFlags(),
				std::size(Shaders::c_text_vert_file_content),
				reinterpret_cast<const uint32_t*>(Shaders::c_text_vert_file_content)));

	shader_frag_=
		vk_device_.createShaderModuleUnique(
			vk::ShaderModuleCreateInfo(
				vk::ShaderModuleCreateFlags(),
				std::size(Shaders::c_text_frag_file_content),
				reinterpret_cast<const uint32_t*>(Shaders::c_text_frag_file_content)));

	// Create image sampler
	font_image_sampler_=
		vk_device_.createSamplerUnique(
			vk::SamplerCreateInfo(
				vk::SamplerCreateFlags(),
				vk::Filter::eLinear,
				vk::Filter::eLinear,
				vk::SamplerMipmapMode::eLinear,
				vk::SamplerAddressMode::eClampToEdge,
				vk::SamplerAddressMode::eClampToEdge,
				vk::SamplerAddressMode::eClampToEdge,
				0.0f,
				VK_FALSE,
				0.0f,
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
			0u,
			vk::DescriptorType::eCombinedImageSampler,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			&*font_image_sampler_,
		},
	};

	descriptor_set_layout_=
		vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(vk_descriptor_set_layout_bindings)), vk_descriptor_set_layout_bindings));

	pipeline_layout_=
		vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*descriptor_set_layout_,
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

	const vk::VertexInputBindingDescription vk_vertex_input_binding_description(
		0u,
		sizeof(TextVertex),
		vk::VertexInputRate::eVertex);

	const vk::VertexInputAttributeDescription vk_vertex_input_attribute_description[]
	{
		{0u, 0u, vk::Format::eR32G32Sfloat , offsetof(TextVertex, pos      )},
		{1u, 0u, vk::Format::eR8G8B8A8Unorm, offsetof(TextVertex, color    )},
		{2u, 0u, vk::Format::eR8G8Uscaled  , offsetof(TextVertex, tex_coord)},
	};

	const vk::PipelineVertexInputStateCreateInfo vk_pipiline_vertex_input_state_create_info(
		vk::PipelineVertexInputStateCreateFlags(),
		1u, &vk_vertex_input_binding_description,
		uint32_t(std::size(vk_vertex_input_attribute_description)), vk_vertex_input_attribute_description);

	const vk::PipelineInputAssemblyStateCreateInfo vk_pipeline_input_assembly_state_create_info(
		vk::PipelineInputAssemblyStateCreateFlags(),
		vk::PrimitiveTopology::eTriangleList);

	const vk::Viewport vk_viewport(0.0f, 0.0f, float(viewport_size_.width), float(viewport_size_.height), 0.0f, 1.0f);
	const vk::Rect2D vk_scissor(vk::Offset2D(0, 0), viewport_size_);

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
		VK_TRUE,
		vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
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

	const vk::PhysicalDeviceMemoryProperties& memory_properties= window_vulkan.GetMemoryProperties();

	const vk::CommandBuffer command_buffer= window_vulkan.BeginFrame();

	// Create font image
	vk::UniqueBuffer image_staging_buffer;
	vk::UniqueDeviceMemory image_staging_buffer_memory;
	{
		const auto image_loaded_opt= Image::Load("mono_font_sdf.tga");
		KK_ASSERT(image_loaded_opt != std::nullopt);
		const Image& image_loaded= *image_loaded_opt;

		glyph_size_[0]= image_loaded.GetWidth();
		glyph_size_[1]= image_loaded.GetHeight() / 96u;

		const uint32_t mip_levels= 4u; // 4 Should be enough.

		font_image_= vk_device_.createImageUnique(
			vk::ImageCreateInfo(
				vk::ImageCreateFlags(),
				vk::ImageType::e2D,
				vk::Format::eR8G8B8A8Unorm,
				vk::Extent3D(image_loaded.GetWidth(), image_loaded.GetHeight(), 1u),
				mip_levels,
				1u,
				vk::SampleCountFlagBits::e1,
				vk::ImageTiling::eOptimal,
				vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
				vk::SharingMode::eExclusive,
				0u, nullptr,
				vk::ImageLayout::eUndefined));

		const vk::MemoryRequirements image_memory_requirements= vk_device_.getImageMemoryRequirements(*font_image_);

		vk::MemoryAllocateInfo vk_memory_allocate_info(image_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((image_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		font_image_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindImageMemory(*font_image_, *font_image_memory_, 0u);

		// Prepare staging buffer.
		image_staging_buffer=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					image_memory_requirements.size,
					vk::BufferUsageFlagBits::eTransferSrc));

		const vk::MemoryRequirements staging_memory_requirements= vk_device_.getBufferMemoryRequirements(*image_staging_buffer);

		vk::MemoryAllocateInfo staging_buffer_allocate_info(staging_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((staging_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible ) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				staging_buffer_allocate_info.memoryTypeIndex= i;
		}

		image_staging_buffer_memory= vk_device_.allocateMemoryUnique(staging_buffer_allocate_info);
		vk_device_.bindBufferMemory(*image_staging_buffer, *image_staging_buffer_memory, 0u);

		void* texture_data_gpu_size= nullptr;
		vk_device_.mapMemory(*image_staging_buffer_memory, 0u, staging_buffer_allocate_info.allocationSize, vk::MemoryMapFlags(), &texture_data_gpu_size);
		std::memcpy(texture_data_gpu_size, image_loaded.GetData(), image_loaded.GetWidth() * image_loaded.GetHeight() * sizeof(Image::PixelType));
		vk_device_.unmapMemory(*image_staging_buffer_memory);

		// Copy staging bufer content into image.

		const vk::ImageMemoryBarrier vk_image_memory_transfer_init(
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eMemoryRead,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
			window_vulkan.GetQueueFamilyIndex(),
			window_vulkan.GetQueueFamilyIndex(),
			*font_image_,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u));

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eBottomOfPipe,
			vk::DependencyFlags(),
			0u, nullptr,
			0u, nullptr,
			1u, &vk_image_memory_transfer_init);

		const vk::BufferImageCopy copy_region(
			0u,
			image_loaded.GetWidth(), image_loaded.GetHeight(),
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0u, 0u, 1u),
			vk::Offset3D(0, 0, 0),
			vk::Extent3D(image_loaded.GetWidth(), image_loaded.GetHeight(), 1u));

		command_buffer.copyBufferToImage(
			*image_staging_buffer,
			*font_image_,
			vk::ImageLayout::eTransferDstOptimal,
			1u, &copy_region);

		for( uint32_t i= 1u; i < mip_levels; ++i)
		{
			// Transform previous mip level layout from dst_optimal to src_optimal
			const vk::ImageMemoryBarrier vk_image_memory_barrier_src(
				vk::AccessFlagBits::eTransferWrite,
				vk::AccessFlagBits::eMemoryRead,
				vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
				window_vulkan.GetQueueFamilyIndex(),
				window_vulkan.GetQueueFamilyIndex(),
				*font_image_,
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, i - 1u, 1u, 0u, 1u));

			command_buffer.pipelineBarrier(
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
				window_vulkan.GetQueueFamilyIndex(),
				window_vulkan.GetQueueFamilyIndex(),
				*font_image_,
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, i, 1u, 0u, 1u));

			command_buffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eBottomOfPipe,
				vk::DependencyFlags(),
				0u, nullptr,
				0u, nullptr,
				1u, &vk_image_memory_barrier_dst);

			std::array<vk::Offset3D, 2> src_offsets;
			std::array<vk::Offset3D, 2> dst_offsets;

			src_offsets[0]= 0u;
			src_offsets[0]= 0u;
			src_offsets[0]= 0u;
			src_offsets[1].x= image_loaded.GetWidth () >> (i-1u);
			src_offsets[1].y= image_loaded.GetHeight() >> (i-1u);
			src_offsets[1].z= 1u;

			dst_offsets[0]= 0u;
			dst_offsets[0]= 0u;
			dst_offsets[0]= 0u;
			dst_offsets[1].x= image_loaded.GetWidth () >> i;
			dst_offsets[1].y= image_loaded.GetHeight() >> i;
			dst_offsets[1].z= 1;

			vk::ImageBlit image_blit(
				vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i - 1u, 0u, 1u),
				src_offsets,
				vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i - 0u, 0u, 1u),
				dst_offsets);

			command_buffer.blitImage(
				*font_image_,
				vk::ImageLayout::eTransferSrcOptimal,
				*font_image_,
				vk::ImageLayout::eTransferDstOptimal,
				1u, &image_blit,
				vk::Filter::eLinear);

			const vk::ImageMemoryBarrier vk_image_memory_barrier_src_final(
				vk::AccessFlagBits::eTransferWrite,
				vk::AccessFlagBits::eMemoryRead,
				vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral,
				window_vulkan.GetQueueFamilyIndex(),
				window_vulkan.GetQueueFamilyIndex(),
				*font_image_,
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, i - 1u, 1u, 0u, 1u));

			command_buffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eBottomOfPipe,
				vk::DependencyFlags(),
				0u, nullptr,
				0u, nullptr,
				1u, &vk_image_memory_barrier_src_final);

			if(i + 1u == mip_levels)
			{
				const vk::ImageMemoryBarrier vk_image_memory_barrier_src_final(
					vk::AccessFlagBits::eTransferWrite,
					vk::AccessFlagBits::eMemoryRead,
					vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral,
					window_vulkan.GetQueueFamilyIndex(),
					window_vulkan.GetQueueFamilyIndex(),
					*font_image_,
					vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, i, 1u, 0u, 1u));

				command_buffer.pipelineBarrier(
					vk::PipelineStageFlagBits::eTransfer,
					vk::PipelineStageFlagBits::eBottomOfPipe,
					vk::DependencyFlags(),
					0u, nullptr,
					0u, nullptr,
					1u, &vk_image_memory_barrier_src_final);
			}
		}

		// Create image view.
		font_image_view_= vk_device_.createImageViewUnique(
			vk::ImageViewCreateInfo(
				vk::ImageViewCreateFlags(),
				*font_image_,
				vk::ImageViewType::e2D,
				vk::Format::eR8G8B8A8Unorm,
				vk::ComponentMapping(vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA),
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, mip_levels, 0u, 1u)));
	}

	// Use device local memory and ise "vkCmdUpdateBuffer, which have limit of 65536  bytes.
	max_glyphs_in_buffer_= 65536u / std::max(sizeof(TextVertex) * 4u, sizeof(uint16_t) * 6u);

	// Create vertex buffer.
	{
		vertex_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					4u * max_glyphs_in_buffer_ * sizeof(TextVertex),
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vertex_buffer_);

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		vertex_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*vertex_buffer_, *vertex_buffer_memory_, 0u);
	}

	// Create index buffer.
	{
		std::vector<uint16_t> quad_indeces(max_glyphs_in_buffer_ * 6u);
		for(uint32_t i= 0u, j= 0u; i < max_glyphs_in_buffer_ * 6; i+= 6u, j+=4u)
		{
			quad_indeces[i]= j;
			quad_indeces[i + 1]= j + 1;
			quad_indeces[i + 2]= j + 2;

			quad_indeces[i + 3]= j + 2;
			quad_indeces[i + 4]= j + 3;
			quad_indeces[i + 5]= j;
		}

		index_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					quad_indeces.size() * sizeof(uint16_t),
					vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*index_buffer_);

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		index_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*index_buffer_, *index_buffer_memory_, 0u);

		command_buffer.updateBuffer(*index_buffer_, 0u, quad_indeces.size() * sizeof(uint16_t), quad_indeces.data());
	}

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
				1u, &*descriptor_set_layout_)).front());

	// Write descriptor set.
	const vk::DescriptorImageInfo descriptor_image_info(
		vk::Sampler(),
		*font_image_view_,
		vk::ImageLayout::eGeneral);

	const vk::WriteDescriptorSet write_descriptor_set(
		*descriptor_set_,
		0u,
		0u,
		1u,
		vk::DescriptorType::eCombinedImageSampler,
		&descriptor_image_info,
		nullptr,
		nullptr);
	vk_device_.updateDescriptorSets(
		1u, &write_descriptor_set,
		0u, nullptr);

	window_vulkan.EndFrame();

	// Wait for image buffer copying.
	vk_device_.waitIdle();
}

TextOut::~TextOut()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

void TextOut::AddText(
	const float column,
	const float row,
	const float size/*in letters*/,
	const uint8_t* const color,
	const char* const text)
{
	AddTextPixelCoords(
		size * float(glyph_size_[0]) * column,
		size * float(glyph_size_[1]) * row,
		size * float(glyph_size_[1]),
		color,
		text);
}

void TextOut::AddTextPixelCoords(
	const float x,
	const float y,
	const float size/*in pixels*/,
	const uint8_t* const color,
	const char* const text)
{
	float cur_x= +2.0f * x / float(viewport_size_.width ) - 1.0f;
	float cur_y= +2.0f * y / float(viewport_size_.height) - 1.0f;
	const float x0= cur_x;

	const float dx= 2.0f * size * float(glyph_size_[0]) / float(viewport_size_.width * glyph_size_[1]);
	const float dy= 2.0f * size / float(viewport_size_.height);

	const char* str= text;
	while(*str != '\0')
	{
		if(*str == '\n')
		{
			cur_x= x0;
			cur_y+= dy;
			++str;
			continue;
		}

		vertices_data_.resize(vertices_data_.size() + 4u);
		TextVertex* const v= vertices_data_.data() + vertices_data_.size() - 4u;

		const uint8_t sym_pos= uint8_t(std::max(0, *str-32));
		v[0].pos[0]= cur_x;
		v[0].pos[1]= cur_y;
		v[0].tex_coord[0]= 0;
		v[0].tex_coord[1]= sym_pos;

		v[1].pos[0]= cur_x;
		v[1].pos[1]= cur_y + dy;
		v[1].tex_coord[0]= 0;
		v[1].tex_coord[1]= sym_pos + 1;

		v[2].pos[0]= cur_x + dx;
		v[2].pos[1]= cur_y + dy;
		v[2].tex_coord[0]= 1;
		v[2].tex_coord[1]= sym_pos + 1;

		v[3].pos[0]= cur_x + dx;
		v[3].pos[1]= cur_y;
		v[3].tex_coord[0]= 1;
		v[3].tex_coord[1]= sym_pos;

		std::memcpy(v[0].color, color, 4u);
		std::memcpy(v[1].color, color, 4u);
		std::memcpy(v[2].color, color, 4u);
		std::memcpy(v[3].color, color, 4u);

		cur_x+= dx;
		++str;
	}
}

void TextOut::Draw(
	const vk::CommandBuffer command_buffer,
	const vk::Framebuffer framebuffer)
{
	const size_t vertex_count= std::min(vertices_data_.size(), max_glyphs_in_buffer_ * 4u);

	command_buffer.updateBuffer(
		*vertex_buffer_,
		0u,
		vertex_count * sizeof(TextVertex),
		vertices_data_.data());

	const vk::ClearValue clear_value(vk::ClearColorValue(std::array<float,4>{0.2f, 0.1f, 0.1f, 0.5f}));

	command_buffer.beginRenderPass(
		vk::RenderPassBeginInfo(
			render_pass_,
			framebuffer,
			vk::Rect2D(vk::Offset2D(0, 0), viewport_size_),
			1u, &clear_value),
			vk::SubpassContents::eInline);

	{
		const vk::DeviceSize offsets= 0u;
		command_buffer.bindVertexBuffers(0u, 1u, &*vertex_buffer_, &offsets);
		command_buffer.bindIndexBuffer(*index_buffer_, 0u, vk::IndexType::eUint16);
		command_buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			*pipeline_layout_,
			0u,
			1u, &*descriptor_set_,
			0u, nullptr);

		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
		command_buffer.drawIndexed(uint32_t(vertex_count / 4u * 6u), 1u, 0u, 0u, 0u);
	}
	command_buffer.endRenderPass();

	vertices_data_.clear();
}

} // namespace KK
