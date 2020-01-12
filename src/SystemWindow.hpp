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
		vk::UniqueFence submit_fence;
	};

	// Use wrapper, because we needs to destroy window in last moment, after destruction of all vulkan objects.
	struct SDLWindowDestroyer
	{
		~SDLWindowDestroyer();
		SDL_Window* w= nullptr;
	};

private:
	void ChangeImageLayout(
		vk::CommandBuffer command_buffer,
		vk::Image image,
		vk::ImageLayout from,
		vk::ImageLayout to);

private:
	// Keep here order of construction.
	SDLWindowDestroyer sdl_window_wrapper_;
	vk::UniqueInstance vk_instance_;
	vk::SurfaceKHR vk_surface_;
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
