#include "Assert.hpp"
#include "SystemWindow.hpp"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <algorithm>
#include <cstring>


namespace KK
{

namespace
{

SystemEventTypes::KeyCode TranslateKey(const SDL_Scancode scan_code)
{
	using KeyCode= SystemEventTypes::KeyCode;

	switch( scan_code )
	{
	case SDL_SCANCODE_ESCAPE: return KeyCode::Escape;
	case SDL_SCANCODE_RETURN: return KeyCode::Enter;
	case SDL_SCANCODE_SPACE: return KeyCode::Space;
	case SDL_SCANCODE_BACKSPACE: return KeyCode::Backspace;
	case SDL_SCANCODE_GRAVE: return KeyCode::BackQuote;
	case SDL_SCANCODE_TAB: return KeyCode::Tab;

	case SDL_SCANCODE_PAGEUP: return KeyCode::PageUp;
	case SDL_SCANCODE_PAGEDOWN: return KeyCode::PageDown;

	case SDL_SCANCODE_RIGHT: return KeyCode::Right;
	case SDL_SCANCODE_LEFT: return KeyCode::Left;
	case SDL_SCANCODE_DOWN: return KeyCode::Down;
	case SDL_SCANCODE_UP: return KeyCode::Up;

	case SDL_SCANCODE_MINUS: return KeyCode::Minus;
	case SDL_SCANCODE_EQUALS: return KeyCode::Equals;

	case SDL_SCANCODE_LEFTBRACKET: return KeyCode::SquareBrackretLeft;
	case SDL_SCANCODE_RIGHTBRACKET: return KeyCode::SquareBrackretRight;

	case SDL_SCANCODE_SEMICOLON: return KeyCode::Semicolon;
	case SDL_SCANCODE_APOSTROPHE: return KeyCode::Apostrophe;
	case SDL_SCANCODE_BACKSLASH: return KeyCode::BackSlash;

	case SDL_SCANCODE_COMMA: return KeyCode::Comma;
	case SDL_SCANCODE_PERIOD: return KeyCode::Period;
	case SDL_SCANCODE_SLASH: return KeyCode::Slash;

	case SDL_SCANCODE_PAUSE: return KeyCode::Pause;

	default:
		if( scan_code >= SDL_SCANCODE_A && scan_code <= SDL_SCANCODE_Z )
			return KeyCode( int(KeyCode::A) + (scan_code - SDL_SCANCODE_A) );
		if( scan_code >= SDL_SCANCODE_1 && scan_code <= SDL_SCANCODE_9 )
			return KeyCode( int(KeyCode::K1) + (scan_code - SDL_SCANCODE_1) );
		if( scan_code == SDL_SCANCODE_0 )
			return KeyCode::K0;
		if( scan_code >= SDL_SCANCODE_F1 && scan_code <= SDL_SCANCODE_F12 )
			return KeyCode( int(KeyCode::F1) + (scan_code - SDL_SCANCODE_F1) );
	};

	return KeyCode::Unknown;
}

SystemEventTypes::ButtonCode TranslateMouseButton(const Uint8 button)
{
	using ButtonCode= SystemEventTypes::ButtonCode;
	switch(button)
	{
	case SDL_BUTTON_LEFT: return ButtonCode::Left;
	case SDL_BUTTON_RIGHT: return ButtonCode::Right;
	case SDL_BUTTON_MIDDLE: return ButtonCode::Middle;
	};

	return ButtonCode::Unknown;
}

uint16_t TranslateKeyModifiers(const Uint16 modifiers)
{
	uint16_t result= 0u;

	if( ( modifiers & ( KMOD_LSHIFT | KMOD_LSHIFT ) ) != 0u )
		result|= SystemEventTypes::KeyModifiers::Shift;

	if( ( modifiers & ( KMOD_LCTRL | KMOD_RCTRL ) ) != 0u )
		result|= SystemEventTypes::KeyModifiers::Control;

	if( ( modifiers & ( KMOD_RALT | KMOD_LALT ) ) != 0u )
		result|= SystemEventTypes::KeyModifiers::Alt;

	if( ( modifiers &  KMOD_CAPS ) != 0u )
		result|= SystemEventTypes::KeyModifiers::Caps;

	return result;
}

} // namespace

SystemWindow::SystemWindow()
{
	// TODO - check errors.
	SDL_Init(SDL_INIT_VIDEO);

	window_=
		SDL_CreateWindow(
			"Klassenkampf",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			800, 600,
			SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);

	unsigned int extension_names_count= 0;
	if( !SDL_Vulkan_GetInstanceExtensions(window_, &extension_names_count, nullptr) )
	{
		std::exit(-1);
	}

	std::vector<const char*> extensions_list;
	extensions_list.resize(extension_names_count, nullptr);

	if( !SDL_Vulkan_GetInstanceExtensions(window_, &extension_names_count, extensions_list.data()) )
	{
		std::exit(-1);
	}

#ifdef DEBUG
	extensions_list.push_back("VK_EXT_debug_report");
	++extension_names_count;
#endif

	VkApplicationInfo vk_app_info;
	std::memset(&vk_app_info, 0, sizeof(vk_app_info));
	vk_app_info.sType= VK_STRUCTURE_TYPE_APPLICATION_INFO;
	vk_app_info.pNext= nullptr;
	vk_app_info.pApplicationName= "Klassenkampf";
	vk_app_info.applicationVersion= VK_MAKE_VERSION(1, 0, 0);
	vk_app_info.pEngineName= "Klassenkampf";
	vk_app_info.engineVersion= VK_MAKE_VERSION(1, 0, 0);
	vk_app_info.apiVersion= VK_MAKE_VERSION(1, 0, 0);

	VkInstanceCreateInfo vk_create_info;
	std::memset(&vk_create_info, 0, sizeof(vk_create_info));
	vk_create_info.sType= VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	vk_create_info.pNext= nullptr;
	vk_create_info.flags= 0;
	vk_create_info.pApplicationInfo= &vk_app_info;
	vk_create_info.enabledExtensionCount= extension_names_count;
	vk_create_info.ppEnabledExtensionNames= extensions_list.data();
	vk_create_info.enabledLayerCount= 0u;
	vk_create_info.ppEnabledLayerNames= nullptr;
#ifdef DEBUG
	{
		uint32_t instance_layer_count= 0u;
		vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr);

		std::vector<VkLayerProperties> vk_layer_properties;
		vk_layer_properties.resize(instance_layer_count);
		vkEnumerateInstanceLayerProperties(&instance_layer_count, vk_layer_properties.data());

		const char* const possible_validation_layers[]{
			"VK_LAYER_LUNARG_core_validation",
			"VK_LAYER_KHRONOS_validation",
		};

		for(const char* const& layer_name : possible_validation_layers)
			for(const VkLayerProperties& property : vk_layer_properties)
				if(std::strcmp(property.layerName, layer_name) == 0)
				{
					vk_create_info.enabledLayerCount= 1u;
					vk_create_info.ppEnabledLayerNames= &layer_name;
					break;
				}
	}
	#endif

	if(vkCreateInstance(&vk_create_info, nullptr, &vk_instance_) != VK_SUCCESS)
	{
		std::exit(-1);
	}

	if(!SDL_Vulkan_CreateSurface(window_, vk_instance_, &vk_surface_))
	{
		std::exit(-1);
	}

	SDL_Vulkan_GetDrawableSize(window_, reinterpret_cast<int*>(&viewport_size_.width), reinterpret_cast<int*>(&viewport_size_.height));

	uint32_t device_count= 0u;
	vkEnumeratePhysicalDevices(vk_instance_, &device_count, nullptr);
	if(device_count == 0u)
	{
		std::exit(-1);
	}

	std::vector<VkPhysicalDevice> physical_devices;
	physical_devices.resize(device_count);
	vkEnumeratePhysicalDevices(vk_instance_, &device_count, physical_devices.data());

	// TODO - select appropriate device or read device id from settings.
	const VkPhysicalDevice physical_device= physical_devices.front();

	uint32_t queue_family_property_count= 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_property_count, nullptr);

	std::vector<VkQueueFamilyProperties> queue_family_properties;
	queue_family_properties.resize(queue_family_property_count);

	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_property_count, queue_family_properties.data());
	uint32_t queue_family_index= ~0u;
	for(uint32_t i= 0u; i < queue_family_property_count; ++i)
	{
		VkBool32 supported= 0;
		vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, vk_surface_, &supported);
		if(supported != 0 &&
			queue_family_properties[i].queueCount > 0 &&
			(queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
		{
			queue_family_index= i;
			break;
		}
	}

	if(queue_family_index == ~0u)
	{
		std::exit(-1);
	}
	vk_queue_familiy_index_= queue_family_index;

	const float queue_priority= 1.0f;
	VkDeviceQueueCreateInfo vk_device_queue_create_info;
	std::memset(&vk_device_queue_create_info, 0, sizeof(vk_device_queue_create_info));
	vk_device_queue_create_info.sType= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	vk_device_queue_create_info.pNext= nullptr;
	vk_device_queue_create_info.flags= 0u;
	vk_device_queue_create_info.queueFamilyIndex= queue_family_index;
	vk_device_queue_create_info.queueCount= 1u;
	vk_device_queue_create_info.pQueuePriorities= &queue_priority;

	const char* const device_extension_names[]{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkDeviceCreateInfo vk_device_create_info;
	std::memset(&vk_device_create_info, 0, sizeof(vk_device_create_info));
	vk_device_create_info.sType= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	vk_device_create_info.queueCreateInfoCount= 1u;
	vk_device_create_info.pQueueCreateInfos= &vk_device_queue_create_info;
	vk_device_create_info.enabledLayerCount= 0u;
	vk_device_create_info.ppEnabledLayerNames= nullptr;
	vk_device_create_info.enabledExtensionCount= uint32_t(std::size(device_extension_names));
	vk_device_create_info.ppEnabledExtensionNames= device_extension_names;
	vk_device_create_info.pEnabledFeatures= nullptr;

	if(vkCreateDevice(physical_device, &vk_device_create_info, nullptr, &vk_device_) != VK_SUCCESS)
	{
		std::exit(-1);
	}

	vkGetDeviceQueue(vk_device_, queue_family_index, 0u, &vk_queue_);

	unsigned int surface_format_count= 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, vk_surface_, &surface_format_count, nullptr);
	if(surface_format_count == 0u)
	{
		std::exit(-1);
	}

	std::vector<VkSurfaceFormatKHR> surface_formats;
	surface_formats.resize(surface_format_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, vk_surface_, &surface_format_count, surface_formats.data());

	VkSurfaceFormatKHR surface_format= surface_formats.back();
	for(const VkSurfaceFormatKHR& surface_format_variant : surface_formats)
	{
		if(surface_format_variant.format == VK_FORMAT_B8G8R8A8_UNORM)
		{
			surface_format= surface_format_variant;
			break;
		}
	}
	swapchain_image_format_= surface_format.format;

	unsigned int present_mode_count= 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, vk_surface_, &present_mode_count, nullptr);
	if(present_mode_count == 0u)
	{
		std::exit(-1);
	}

	std::vector<VkPresentModeKHR> present_modes;
	present_modes.resize(present_mode_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, vk_surface_, &present_mode_count, present_modes.data());

	VkPresentModeKHR present_mode= present_modes.front();
	if(std::find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != present_modes.end())
		present_mode= VK_PRESENT_MODE_MAILBOX_KHR; // Use tripple buffering.
	else if(std::find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_FIFO_KHR) != present_modes.end())
		present_mode= VK_PRESENT_MODE_FIFO_KHR; // Use double buffering.

	VkSurfaceCapabilitiesKHR surface_capabilities;
	if(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, vk_surface_, &surface_capabilities) != VK_SUCCESS)
	{
		std::exit(-1);
	}

	VkSwapchainCreateInfoKHR vk_swapchain_create_info;
	std::memset(&vk_swapchain_create_info, 0, sizeof(vk_swapchain_create_info));
	vk_swapchain_create_info.sType= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	vk_swapchain_create_info.pNext= nullptr;
	vk_swapchain_create_info.flags= 0u;
	vk_swapchain_create_info.surface= vk_surface_;
	vk_swapchain_create_info.minImageCount= surface_capabilities.minImageCount;
	vk_swapchain_create_info.imageFormat= surface_format.format;
	vk_swapchain_create_info.imageColorSpace= surface_format.colorSpace;
	vk_swapchain_create_info.imageExtent= surface_capabilities.maxImageExtent;
	vk_swapchain_create_info.imageArrayLayers= 1u;
	vk_swapchain_create_info.imageUsage= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	vk_swapchain_create_info.imageSharingMode= VK_SHARING_MODE_EXCLUSIVE;
	vk_swapchain_create_info.queueFamilyIndexCount= 1u;
	vk_swapchain_create_info.pQueueFamilyIndices= &queue_family_index;
	vk_swapchain_create_info.preTransform= surface_capabilities.currentTransform;
	vk_swapchain_create_info.compositeAlpha= VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	vk_swapchain_create_info.presentMode= present_mode;
	vk_swapchain_create_info.clipped= VK_TRUE;

	if(vkCreateSwapchainKHR(vk_device_, &vk_swapchain_create_info, nullptr, &vk_swapchain_) != VK_SUCCESS)
	{
		std::exit(-1);
	}

	uint32_t swapchain_images_count= 0u;
	vkGetSwapchainImagesKHR(vk_device_, vk_swapchain_, &swapchain_images_count, nullptr);
	if(swapchain_images_count == 0u)
	{
		std::exit(-1);
	}
	vk_swapchain_images_.resize(swapchain_images_count);
	vkGetSwapchainImagesKHR(vk_device_, vk_swapchain_, &swapchain_images_count, vk_swapchain_images_.data());

	vk_swapchain_images_view_.resize(swapchain_images_count);
	for(uint32_t i= 0u; i < swapchain_images_count; ++i)
	{
		VkImageViewCreateInfo vk_image_view_create_info;
		std::memset(&vk_image_view_create_info, 0, sizeof(vk_image_view_create_info));
		vk_image_view_create_info.sType= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		vk_image_view_create_info.pNext= nullptr;
		vk_image_view_create_info.flags= 0u;
		vk_image_view_create_info.image= vk_swapchain_images_[i];
		vk_image_view_create_info.viewType= VK_IMAGE_VIEW_TYPE_2D;
		vk_image_view_create_info.format= swapchain_image_format_;
		vk_image_view_create_info.components.r= VK_COMPONENT_SWIZZLE_IDENTITY;
		vk_image_view_create_info.components.g= VK_COMPONENT_SWIZZLE_IDENTITY;
		vk_image_view_create_info.components.b= VK_COMPONENT_SWIZZLE_IDENTITY;
		vk_image_view_create_info.components.a= VK_COMPONENT_SWIZZLE_IDENTITY;
		vk_image_view_create_info.subresourceRange.aspectMask= VK_IMAGE_ASPECT_COLOR_BIT;
		vk_image_view_create_info.subresourceRange.baseMipLevel= 0u;
		vk_image_view_create_info.subresourceRange.levelCount= 1u;
		vk_image_view_create_info.subresourceRange.baseArrayLayer= 0u;
		vk_image_view_create_info.subresourceRange.layerCount= 1u;
		vkCreateImageView(vk_device_, &vk_image_view_create_info, nullptr, &vk_swapchain_images_view_[i]);
	}

	VkCommandPoolCreateInfo vk_command_pool_create_info;
	std::memset(&vk_command_pool_create_info, 0, sizeof(vk_command_pool_create_info));
	vk_command_pool_create_info.sType= VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	vk_command_pool_create_info.pNext= nullptr;
	vk_command_pool_create_info.flags= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	vk_command_pool_create_info.queueFamilyIndex= queue_family_index;

	if(vkCreateCommandPool(vk_device_, &vk_command_pool_create_info, nullptr, &vk_command_pool_) != VK_SUCCESS)
	{
		std::exit(-1);
	}

	frames_data_.resize(16u); // TODO - maybe use less frame data?
	for(FrameData& frame_data : frames_data_)
	{
		VkCommandBufferAllocateInfo vk_command_buffer_allocate_info;
		std::memset(&vk_command_buffer_allocate_info, 0, sizeof(vk_command_buffer_allocate_info));
		vk_command_buffer_allocate_info.sType= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		vk_command_buffer_allocate_info.pNext= nullptr;
		vk_command_buffer_allocate_info.commandPool= vk_command_pool_;
		vk_command_buffer_allocate_info.level= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		vk_command_buffer_allocate_info.commandBufferCount= 1u;

		vkAllocateCommandBuffers(vk_device_, &vk_command_buffer_allocate_info, &frame_data.command_buffer);

		VkSemaphoreCreateInfo vk_semaphore_create_info;
		std::memset(&vk_semaphore_create_info, 0, sizeof(vk_semaphore_create_info));
		vk_semaphore_create_info.sType= VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		vk_semaphore_create_info.pNext= nullptr;
		vkCreateSemaphore(vk_device_, &vk_semaphore_create_info, nullptr, &frame_data.image_available_semaphore);
		vkCreateSemaphore(vk_device_, &vk_semaphore_create_info, nullptr, &frame_data.rendering_finished_semaphore);
	}

	return;
}

SystemWindow::~SystemWindow()
{
	for(const FrameData& frame_data : frames_data_)
	{
		vkFreeCommandBuffers(vk_device_, vk_command_pool_, 1u, &frame_data.command_buffer);
		vkDestroySemaphore(vk_device_, frame_data.image_available_semaphore, nullptr);
		vkDestroySemaphore(vk_device_, frame_data.rendering_finished_semaphore, nullptr);
	}

	for(const VkImageView image_view : vk_swapchain_images_view_)
		vkDestroyImageView(vk_device_, image_view, nullptr);

	vkDestroyCommandPool(vk_device_, vk_command_pool_, nullptr);
	vkDestroySwapchainKHR(vk_device_, vk_swapchain_, nullptr);
	vkDestroyDevice(vk_device_, nullptr);
	vkDestroySurfaceKHR(vk_instance_, vk_surface_, nullptr);
	vkDestroyInstance(vk_instance_, nullptr);
	SDL_DestroyWindow(window_);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

SystemEvents SystemWindow::ProcessEvents()
{
	SystemEvents result_events;

	SDL_Event event;
	while( SDL_PollEvent(&event) )
	{
		switch(event.type)
		{
		case SDL_WINDOWEVENT:
			if(event.window.event == SDL_WINDOWEVENT_CLOSE)
				result_events.emplace_back(SystemEventTypes::QuitEvent());
			break;

		case SDL_QUIT:
			result_events.emplace_back(SystemEventTypes::QuitEvent());
			break;

		case SDL_KEYUP:
		case SDL_KEYDOWN:
			{
				SystemEventTypes::KeyEvent key_event;
				key_event.key_code= TranslateKey(event.key.keysym.scancode);
				key_event.pressed= event.type == SDL_KEYUP ? false : true;
				key_event.modifiers= TranslateKeyModifiers(event.key.keysym.mod);
				result_events.emplace_back(key_event);
			}
			break;

		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEBUTTONDOWN:
			{
				SystemEventTypes::MouseKeyEvent mouse_event;
				mouse_event.mouse_button= TranslateMouseButton(event.button.button);
				mouse_event.x= uint16_t(event.button.x);
				mouse_event.y= uint16_t(event.button.y);
				mouse_event.pressed= event.type == SDL_MOUSEBUTTONUP ? false : true;
				result_events.emplace_back(mouse_event);
			}
			break;

		case SDL_MOUSEMOTION:
			{
				SystemEventTypes::MouseMoveEvent mouse_event;
				mouse_event.dx= uint16_t(event.motion.xrel);
				mouse_event.dy= uint16_t(event.motion.yrel);
				result_events.emplace_back(mouse_event);
			}
			break;

		case SDL_TEXTINPUT:
			// Accept only visible ASCII
			if( event.text.text[0] >= 32 )
			{
				SystemEventTypes::CharInputEvent char_event;
				char_event.ch= event.text.text[0];
				result_events.emplace_back(char_event);
			}
			break;

		// TODO - fill other events here

		default:
			break;
		};
	}

	return result_events;
}

VkCommandBuffer SystemWindow::BeginFrame()
{
	current_frame_data_= &frames_data_[frame_count_ % frames_data_.size()];
	++frame_count_;

	vkAcquireNextImageKHR(vk_device_, vk_swapchain_, std::numeric_limits<uint64_t>::max(), current_frame_data_->image_available_semaphore, nullptr, &current_swapchain_image_index_);

	const VkCommandBuffer command_buffer= current_frame_data_->command_buffer;

	VkCommandBufferBeginInfo vk_command_buffer_begin_info;
	std::memset(&vk_command_buffer_begin_info, 0, sizeof(vk_command_buffer_begin_info));
	vk_command_buffer_begin_info.sType= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vk_command_buffer_begin_info.pNext= nullptr;
	vk_command_buffer_begin_info.flags= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(command_buffer, &vk_command_buffer_begin_info);

	ClearScreen(command_buffer);

	return command_buffer;
}

void SystemWindow::EndFrame()
{
	vkEndCommandBuffer(current_frame_data_->command_buffer);

	const uint32_t wait_dst_stage_mask= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo vk_submit_info;
	std::memset(&vk_submit_info, 0, sizeof(vk_submit_info));
	vk_submit_info.sType= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	vk_submit_info.pNext= nullptr;
	vk_submit_info.waitSemaphoreCount= 1u;
	vk_submit_info.pWaitSemaphores= &current_frame_data_->image_available_semaphore;
	vk_submit_info.pWaitDstStageMask= &wait_dst_stage_mask;
	vk_submit_info.commandBufferCount= 1u;
	vk_submit_info.pCommandBuffers= &current_frame_data_->command_buffer;
	vk_submit_info.signalSemaphoreCount= 1u;
	vk_submit_info.pSignalSemaphores= &current_frame_data_->rendering_finished_semaphore;

	vkQueueSubmit(vk_queue_, 1u, &vk_submit_info, nullptr);

	VkPresentInfoKHR vk_present_info;
	std::memset(&vk_present_info, 0, sizeof(vk_present_info));
	vk_present_info.sType= VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	vk_present_info.pNext= nullptr;
	vk_present_info.waitSemaphoreCount= 1u;
	vk_present_info.pWaitSemaphores= &current_frame_data_->rendering_finished_semaphore;
	vk_present_info.swapchainCount= 1u;
	vk_present_info.pSwapchains= &vk_swapchain_;
	vk_present_info.pImageIndices= &current_swapchain_image_index_;
	vk_present_info.pResults= nullptr;

	vkQueuePresentKHR(vk_queue_, &vk_present_info);
	vkDeviceWaitIdle(vk_device_);
}

void SystemWindow::ClearScreen(const VkCommandBuffer command_buffer)
{
	VkImageSubresourceRange vk_image_range;
	std::memset(&vk_image_range, 0, sizeof(vk_image_range));
	vk_image_range.aspectMask= VK_IMAGE_ASPECT_COLOR_BIT;
	vk_image_range.baseMipLevel= 0u;
	vk_image_range.layerCount= 1u;
	vk_image_range.baseArrayLayer= 0u;
	vk_image_range.levelCount= 1u;

	VkImageMemoryBarrier vk_image_memory_barrier;
	std::memset(&vk_image_memory_barrier, 0, sizeof(vk_image_memory_barrier));
	vk_image_memory_barrier.sType= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	vk_image_memory_barrier.srcAccessMask= VK_ACCESS_TRANSFER_WRITE_BIT;
	vk_image_memory_barrier.dstAccessMask= VK_ACCESS_MEMORY_READ_BIT;
	vk_image_memory_barrier.oldLayout= VK_IMAGE_LAYOUT_UNDEFINED;
	vk_image_memory_barrier.newLayout= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	vk_image_memory_barrier.srcQueueFamilyIndex= vk_queue_familiy_index_;
	vk_image_memory_barrier.dstQueueFamilyIndex= vk_queue_familiy_index_;
	vk_image_memory_barrier.image= vk_swapchain_images_[current_swapchain_image_index_];
	vk_image_memory_barrier.subresourceRange= vk_image_range;

	vkCmdPipelineBarrier(
		command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0u, 0u, nullptr, 0u, nullptr, 1u,
		&vk_image_memory_barrier);
}

VkDevice SystemWindow::GetVulkanDevice() const
{
	return vk_device_;
}

VkFormat SystemWindow::GetSurfaceFormat() const
{
	return swapchain_image_format_;
}

VkExtent2D SystemWindow::GetViewportSize() const
{
	return viewport_size_;
}

const std::vector<VkImageView>& SystemWindow::GetSwapchainImagesViews() const
{
	return vk_swapchain_images_view_;
}

size_t SystemWindow::GetCurrentSwapchainImageIndex() const
{
	return current_swapchain_image_index_;
}

} // namespace KK
