#pragma once
#include <vulkan/vulkan.h>
#include <vector>


namespace KK
{

class WorldRenderer final
{
public:
	WorldRenderer(
		VkDevice vk_device,
		VkFormat surface_format,
		const std::vector<VkImageView>& swapchain_image_views);

	~WorldRenderer();

	void Draw(VkCommandBuffer command_buffer);

private:
	const VkDevice vk_device_;

	VkRenderPass vk_render_pass_= nullptr;
	std::vector<VkFramebuffer> vk_framebuffers_;
};

} // namespace KK
