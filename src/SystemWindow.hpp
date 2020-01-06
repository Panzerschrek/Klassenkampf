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

	void BeginFrame();
	void EndFrame();

private:

	SDL_Window* window_= nullptr;
	VkInstance vk_instance_= nullptr;
	VkSurfaceKHR vk_surface_= nullptr;
	VkDevice vk_device_= nullptr;
	VkQueue vk_queue_= nullptr;
};

} // namespace KK
