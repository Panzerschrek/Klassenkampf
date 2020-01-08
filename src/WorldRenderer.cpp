#include "WorldRenderer.hpp"
#include <cstring>


namespace KK
{

WorldRenderer::WorldRenderer(
	const VkDevice vk_device,
	const VkFormat surface_format,
	const std::vector<VkImageView>& swapchain_image_views)
	: vk_device_(vk_device)
{
	VkAttachmentDescription vk_attachment_description;
	std::memset(&vk_attachment_description, 0, sizeof(vk_attachment_description));
	vk_attachment_description.flags= 0u;
	vk_attachment_description.format= surface_format;
	vk_attachment_description.samples= VK_SAMPLE_COUNT_1_BIT; // TODO - maybe use 0?
	vk_attachment_description.loadOp= VK_ATTACHMENT_LOAD_OP_CLEAR;
	vk_attachment_description.storeOp= VK_ATTACHMENT_STORE_OP_STORE;
	vk_attachment_description.stencilLoadOp= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	vk_attachment_description.stencilStoreOp= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	vk_attachment_description.initialLayout= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	vk_attachment_description.finalLayout= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference vk_attachment_reference;
	std::memset(&vk_attachment_reference, 0, sizeof(vk_attachment_reference));
	vk_attachment_reference.attachment= 0u;
	vk_attachment_reference.layout= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription vk_subpass_description;
	std::memset(&vk_subpass_description, 0, sizeof(vk_subpass_description));
	vk_subpass_description.flags= 0u;
	vk_subpass_description.pipelineBindPoint= VK_PIPELINE_BIND_POINT_GRAPHICS;
	vk_subpass_description.inputAttachmentCount= 0u;
	vk_subpass_description.pInputAttachments= nullptr;
	vk_subpass_description.colorAttachmentCount= 1u;
	vk_subpass_description.pColorAttachments= &vk_attachment_reference;
	vk_subpass_description.pResolveAttachments= nullptr;
	vk_subpass_description.pDepthStencilAttachment= nullptr;
	vk_subpass_description.preserveAttachmentCount= 0u;
	vk_subpass_description.pPreserveAttachments= nullptr;

	VkRenderPassCreateInfo vk_render_pass_create_info;
	std::memset(&vk_render_pass_create_info, 0, sizeof(vk_render_pass_create_info));
	vk_render_pass_create_info.sType= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	vk_render_pass_create_info.pNext= nullptr;
	vk_render_pass_create_info.flags= 0u;
	vk_render_pass_create_info.attachmentCount= 1u;
	vk_render_pass_create_info.pAttachments= &vk_attachment_description;
	vk_render_pass_create_info.subpassCount= 1u;
	vk_render_pass_create_info.pSubpasses= &vk_subpass_description;

	vkCreateRenderPass(vk_device_, &vk_render_pass_create_info, nullptr, &vk_render_pass_);

	vk_framebuffers_.resize(swapchain_image_views.size());
	for(size_t i= 0u; i < swapchain_image_views.size(); ++i)
	{
		VkFramebufferCreateInfo vk_framebuffer_create_info;
		std::memset(&vk_framebuffer_create_info, 0, sizeof(vk_framebuffer_create_info));
		vk_framebuffer_create_info.sType= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		vk_framebuffer_create_info.pNext= nullptr;
		vk_framebuffer_create_info.flags= 0u;
		vk_framebuffer_create_info.renderPass= vk_render_pass_;
		vk_framebuffer_create_info.attachmentCount= 1u;
		vk_framebuffer_create_info.pAttachments= &swapchain_image_views[i];
		vk_framebuffer_create_info.width= 800u;
		vk_framebuffer_create_info.height= 600u;
		vk_framebuffer_create_info.layers= 1u;

		vkCreateFramebuffer(vk_device_, &vk_framebuffer_create_info, nullptr, &vk_framebuffers_[i]);
	}
}

WorldRenderer::~WorldRenderer()
{
	for(const VkFramebuffer framebuffer : vk_framebuffers_)
	{
		vkDestroyFramebuffer(vk_device_, framebuffer, nullptr);
	}

	vkDestroyRenderPass(vk_device_, vk_render_pass_, nullptr);
}

void WorldRenderer::Draw(const VkCommandBuffer command_buffer)
{
	(void)command_buffer;
}

} // namespace KK
