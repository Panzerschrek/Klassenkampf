#pragma once
#include "SystemEvent.hpp"
#include <SDL_video.h>
#include <vulkan/vulkan.h>


namespace KK
{

class SystemWindow final
{
public:
	SystemWindow();
	~SystemWindow();

	SystemEvents ProcessEvents();

	VkCommandBuffer BeginFrame();
	void EndFrame();

private:
	struct FrameData
	{
		VkCommandBuffer command_buffer;
		VkSemaphore image_available_semaphore;
		VkSemaphore rendering_finished_semaphore;
	};

private:
	void ClearScreen(VkCommandBuffer command_buffer);

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

	std::vector<FrameData> frames_data_;
	const FrameData* current_frame_data_= nullptr;
	size_t frame_count_= 0u;
	uint32_t current_swapchain_image_index_= ~0u;
};

} // namespace KK
