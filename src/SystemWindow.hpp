#pragma once
#include "SystemEvent.hpp"
#include <SDL_video.h>
#include <vulkan/vulkan.h>
#include <queue>


namespace KK
{

class SystemWindow final
{
public:
	SystemWindow();
	~SystemWindow();

	SystemEvents ProcessEvents();

	void BeginFrame();
	void EndFrame();

private:
	SDL_Window* window_= nullptr;
	VkInstance vk_instance_= nullptr;
	VkSurfaceKHR vk_surface_= nullptr;
	VkDevice vk_device_= nullptr;
	VkQueue vk_queue_= nullptr;
	uint32_t vk_queue_familiy_index_= ~0u;
	VkSwapchainKHR vk_swapchain_= nullptr;
	std::vector<VkImage> vk_swapchain_images_;
	VkCommandPool vk_command_pool_= nullptr;

	VkSemaphore vk_rendering_finished_semaphore_= nullptr;
	VkSemaphore vk_image_available_semaphore_= nullptr;

	std::queue<VkCommandBuffer> vk_command_buffers_;
};

} // namespace KK
