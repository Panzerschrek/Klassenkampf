#pragma once
#include <vulkan/vulkan.h>


namespace KK
{

class WorldRenderer final
{
public:
	WorldRenderer(VkDevice vk_device, VkFormat surface_format);
	~WorldRenderer();

	void Draw(VkCommandBuffer command_buffer);

private:
	const VkDevice vk_device_;

	VkRenderPass vk_render_pass_= nullptr;
};

} // namespace KK
