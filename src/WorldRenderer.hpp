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
		VkExtent2D viewport_size,
		const std::vector<VkImageView>& swapchain_image_views);

	~WorldRenderer();

	void Draw(VkCommandBuffer command_buffer, size_t swapchain_image_index, float frame_time_s);

private:
	const VkDevice vk_device_;
	const VkExtent2D viewport_size_;

	VkRenderPass vk_render_pass_= nullptr;
	std::vector<VkFramebuffer> vk_framebuffers_;

	VkShaderModule shader_vert_= nullptr;
	VkShaderModule shader_frag_= nullptr;
	VkDescriptorSetLayout vk_decriptor_set_layout_= nullptr;
	VkPipelineLayout vk_pipeline_layout_= nullptr;
	VkPipeline vk_pipeline_= nullptr;
};

} // namespace KK
