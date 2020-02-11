#include "Shadowmapper.hpp"
#include "Log.hpp"


namespace KK
{

Shadowmapper::Shadowmapper(WindowVulkan& window_vulkan)
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

	shadowmap_size_.width = 2048u;
	shadowmap_size_.height= 2048u;

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
					vk::ImageUsageFlagBits::eDepthStencilAttachment,
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
}

Shadowmapper::~Shadowmapper()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

} // namespace KK
