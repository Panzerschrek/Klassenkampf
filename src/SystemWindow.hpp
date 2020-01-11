#pragma once
#include "SystemEvent.hpp"
#include <SDL_video.h>
#include <vulkan/vulkan.hpp>


namespace KK
{

class SystemWindow final
{
public:
	SystemWindow();
	~SystemWindow();

	SystemEvents ProcessEvents();

	vk::CommandBuffer BeginFrame();
	void EndFrame();

	vk::Device GetVulkanDevice() const;
	vk::Format GetSurfaceFormat() const;
	vk::Extent2D GetViewportSize() const;
	const vk::PhysicalDeviceMemoryProperties& GetMemoryProperties() const;
	const std::vector<vk::UniqueImageView>& GetSwapchainImagesViews() const;
	size_t GetCurrentSwapchainImageIndex() const;

private:
	struct FrameData
	{
		vk::UniqueCommandBuffer command_buffer;
		vk::UniqueSemaphore image_available_semaphore;
		vk::UniqueSemaphore rendering_finished_semaphore;
	};

private:
	void ClearScreen(vk::CommandBuffer command_buffer);

private:
	SDL_Window* window_= nullptr;
	vk::UniqueInstance vk_instance_;
	vk::UniqueSurfaceKHR vk_surface_;
	vk::UniqueDevice vk_device_;
	vk::Queue vk_queue_= nullptr;
	uint32_t vk_queue_familiy_index_= ~0u;
	vk::Extent2D viewport_size_;
	vk::PhysicalDeviceMemoryProperties memory_properties_;
	vk::UniqueSwapchainKHR vk_swapchain_;
	std::vector<vk::Image> vk_swapchain_images_;
	std::vector<vk::UniqueImageView> vk_swapchain_images_view_;
	vk::Format swapchain_image_format_= vk::Format::eUndefined;
	vk::UniqueCommandPool vk_command_pool_;

	std::vector<FrameData> frames_data_;
	const FrameData* current_frame_data_= nullptr;
	size_t frame_count_= 0u;
	uint32_t current_swapchain_image_index_= ~0u;
};

} // namespace KK
