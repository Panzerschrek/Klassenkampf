#pragma once
#include "SystemWindow.hpp"
#include <vulkan/vulkan.hpp>


namespace KK
{

class WindowVulkan final
{
public:
	explicit WindowVulkan(const SystemWindow& system_window);
	~WindowVulkan();

	vk::CommandBuffer BeginFrame();
	void EndFrame();

	vk::Device GetVulkanDevice() const;
	vk::Extent2D GetViewportSize() const;
	uint32_t GetQueueFamilyIndex() const;
	vk::RenderPass GetRenderPass() const; // Render pass for rendering directly into screen.
	const vk::PhysicalDeviceMemoryProperties& GetMemoryProperties() const;
	vk::Framebuffer GetCurrentFramebuffer() const; // Current screen framebuffer.

private:
	struct CommandBufferData
	{
		vk::UniqueCommandBuffer command_buffer;
		vk::UniqueSemaphore image_available_semaphore;
		vk::UniqueSemaphore rendering_finished_semaphore;
		vk::UniqueFence submit_fence;
	};

	struct SwapchainFramebufferData
	{
		vk::Image image;
		vk::UniqueImageView image_view;
		vk::UniqueFramebuffer framebuffer;
	};

private:
	void ChangeImageLayout(
		vk::CommandBuffer command_buffer,
		vk::Image image,
		vk::ImageLayout from,
		vk::ImageLayout to);

private:
	// Keep here order of construction.
	vk::UniqueInstance vk_instance_;
	vk::SurfaceKHR vk_surface_;
	vk::UniqueDevice vk_device_;
	vk::Queue vk_queue_= nullptr;
	uint32_t vk_queue_family_index_= ~0u;
	vk::Extent2D viewport_size_;
	vk::PhysicalDeviceMemoryProperties memory_properties_;
	vk::UniqueSwapchainKHR vk_swapchain_;

	vk::UniqueRenderPass vk_render_pass_;
	std::vector<SwapchainFramebufferData> framebuffers_; // one framebuffer for each swapchain image.

	vk::UniqueCommandPool vk_command_pool_;

	std::vector<CommandBufferData> command_buffers_;
	const CommandBufferData* current_frame_command_buffer_= nullptr;
	size_t frame_count_= 0u;
	uint32_t current_swapchain_image_index_= ~0u;
};

} // namespace KK
