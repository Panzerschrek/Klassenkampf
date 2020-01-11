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

SystemWindow::SDLWindowDestroyer::~SDLWindowDestroyer()
{
	if(w != nullptr)
	{
		SDL_DestroyWindow(w);
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
}

SystemWindow::SystemWindow()
{
	#ifdef DEBUG
	const bool use_debug_extensions_and_layers= true;
	#else
	const bool use_debug_extensions_and_layers= true;
	#endif

	// TODO - check errors.
	SDL_Init(SDL_INIT_VIDEO);

	sdl_window_wrapper_.w=
		SDL_CreateWindow(
			"Klassenkampf",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			800, 600,
			SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);

	// Get vulkan extensiion, needed by SDL.
	unsigned int extension_names_count= 0;
	if( !SDL_Vulkan_GetInstanceExtensions(sdl_window_wrapper_.w, &extension_names_count, nullptr) )
	{
		std::exit(-1);
	}

	std::vector<const char*> extensions_list;
	extensions_list.resize(extension_names_count, nullptr);

	if( !SDL_Vulkan_GetInstanceExtensions(sdl_window_wrapper_.w, &extension_names_count, extensions_list.data()) )
	{
		std::exit(-1);
	}

	if(use_debug_extensions_and_layers)
	{
		extensions_list.push_back("VK_EXT_debug_report");
		++extension_names_count;
	}

	// Create Vulkan instance.
	const vk::ApplicationInfo vk_app_info(
		"Klassenkampf",
		VK_MAKE_VERSION(0, 0, 1),
		"Klassenkampf",
		VK_MAKE_VERSION(0, 0, 1),
		VK_MAKE_VERSION(1, 0, 0));

	vk::InstanceCreateInfo vk_instance_create_info(
		vk::InstanceCreateFlags(),
		&vk_app_info,
		0u, nullptr,
		extension_names_count, extensions_list.data());

	if(use_debug_extensions_and_layers)
	{
		const std::vector<vk::LayerProperties> vk_layer_properties= vk::enumerateInstanceLayerProperties();
		const char* const possible_validation_layers[]
		{
			"VK_LAYER_LUNARG_core_validation",
			"VK_LAYER_KHRONOS_validation",
		};

		for(const char* const& layer_name : possible_validation_layers)
			for(const vk::LayerProperties& property : vk_layer_properties)
				if(std::strcmp(property.layerName, layer_name) == 0)
				{
					vk_instance_create_info.enabledLayerCount= 1u;
					vk_instance_create_info.ppEnabledLayerNames= &layer_name;
					break;
				}
	}

	vk_instance_= vk::createInstanceUnique(vk_instance_create_info);

	// Create surface.
	VkSurfaceKHR vk_tmp_surface;
	if(!SDL_Vulkan_CreateSurface(sdl_window_wrapper_.w, *vk_instance_, &vk_tmp_surface))
	{
		std::exit(-1);
	}
	vk_surface_= vk_tmp_surface;

	SDL_Vulkan_GetDrawableSize(sdl_window_wrapper_.w, reinterpret_cast<int*>(&viewport_size_.width), reinterpret_cast<int*>(&viewport_size_.height));

	// Create physical device. Prefer usage of discrete GPU. TODO - allow user to select device.
	const std::vector<vk::PhysicalDevice> physical_devices= vk_instance_->enumeratePhysicalDevices();
	vk::PhysicalDevice physical_device= physical_devices.front();
	if(physical_devices.size() > 1u)
	{
		for(const vk::PhysicalDevice& physical_device_candidate : physical_devices)
		{
			const vk::PhysicalDeviceProperties properties= physical_device_candidate.getProperties();
			if(properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			{
				physical_device= physical_device_candidate;
				break;
			}
		}
	}

	// Select queue family.
	const std::vector<vk::QueueFamilyProperties> queue_family_properties= physical_device.getQueueFamilyProperties();
	uint32_t queue_family_index= ~0u;
	for(uint32_t i= 0u; i < queue_family_properties.size(); ++i)
	{
		const VkBool32 supported= physical_device.getSurfaceSupportKHR(i, vk_surface_);
		if(supported != 0 &&
			queue_family_properties[i].queueCount > 0 &&
			(queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlagBits(0))
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

	const vk::DeviceQueueCreateInfo vk_device_queue_create_info(
		vk::DeviceQueueCreateFlags(),
		queue_family_index,
		1u);

	const char* const device_extension_names[]{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	const vk::DeviceCreateInfo vk_device_create_info(
		vk::DeviceCreateFlags(),
		1u, &vk_device_queue_create_info,
		0u, nullptr,
		uint32_t(std::size(device_extension_names)), device_extension_names);

	// Create physical device.
	// HACK! createDeviceUnique works wrong! Use other method instead.
	//vk_device_= physical_device.createDeviceUnique(vk_device_create_info);
	vk::Device vk_device_tmp;
	if((physical_device.createDevice(&vk_device_create_info, nullptr, &vk_device_tmp)) != vk::Result::eSuccess)
	{
		std::exit(-1);
	}
	vk_device_.reset(vk_device_tmp);

	vk_queue_= vk_device_->getQueue(queue_family_index, 0u);

	// Select surface format. Prefer usage of normalized rbga32.
	const std::vector<vk::SurfaceFormatKHR> surface_formats= physical_device.getSurfaceFormatsKHR(vk_surface_);
	vk::SurfaceFormatKHR surface_format= surface_formats.back();
	for(const vk::SurfaceFormatKHR& surface_format_variant : surface_formats)
	{
		if(surface_format_variant.format == vk::Format::eR8G8B8A8Unorm)
		{
			surface_format= surface_format_variant;
			break;
		}
	}
	swapchain_image_format_= surface_format.format;

	// Select present mode. Prefer usage of tripple buffering, than double buffering.
	const std::vector<vk::PresentModeKHR> present_modes= physical_device.getSurfacePresentModesKHR(vk_surface_);
	vk::PresentModeKHR present_mode= present_modes.front();
	if(std::find(present_modes.begin(), present_modes.end(), vk::PresentModeKHR::eMailbox) != present_modes.end())
		present_mode= vk::PresentModeKHR::eMailbox; // Use tripple buffering.
	else if(std::find(present_modes.begin(), present_modes.end(), vk::PresentModeKHR::eFifo) != present_modes.end())
		present_mode= vk::PresentModeKHR::eFifo; // Use double buffering.

	const vk::SurfaceCapabilitiesKHR surface_capabilities= physical_device.getSurfaceCapabilitiesKHR(vk_surface_);

	vk_swapchain_= vk_device_->createSwapchainKHRUnique(
		vk::SwapchainCreateInfoKHR(
			vk::SwapchainCreateFlagsKHR(),
			vk_surface_,
			surface_capabilities.minImageCount,
			surface_format.format,
			surface_format.colorSpace,
			surface_capabilities.maxImageExtent,
			1u,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
			vk::SharingMode::eExclusive,
			1u, &queue_family_index,
			vk::SurfaceTransformFlagBitsKHR::eIdentity,
			vk::CompositeAlphaFlagBitsKHR::eOpaque,
			present_mode));

	// Get images and create images view (for further framebuffers creation).
	vk_swapchain_images_= vk_device_->getSwapchainImagesKHR(*vk_swapchain_);

	vk_swapchain_images_view_.resize(vk_swapchain_images_.size());
	for(size_t i= 0u; i < vk_swapchain_images_.size(); ++i)
	{
		vk_swapchain_images_view_[i]=
			vk_device_->createImageViewUnique(
				vk::ImageViewCreateInfo(
					vk::ImageViewCreateFlags(),
					vk_swapchain_images_[i],
					vk::ImageViewType::e2D,
					swapchain_image_format_,
					vk::ComponentMapping(),
					vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u)));
	}

	// Create command pull.
	vk_command_pool_= vk_device_->createCommandPoolUnique(
		vk::CommandPoolCreateInfo(
			vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			queue_family_index));

	// Create command buffers and it's synchronization primitives.
	frames_data_.resize(3u); // Use tripple buffering for command buffers.
	for(FrameData& frame_data : frames_data_)
	{
		frame_data.command_buffer=
			std::move(
			vk_device_->allocateCommandBuffersUnique(
				vk::CommandBufferAllocateInfo(
					*vk_command_pool_,
					vk::CommandBufferLevel::ePrimary,
					1u)).front());

		frame_data.image_available_semaphore= vk_device_->createSemaphoreUnique(vk::SemaphoreCreateInfo());
		frame_data.rendering_finished_semaphore= vk_device_->createSemaphoreUnique(vk::SemaphoreCreateInfo());
		frame_data.submit_fence= vk_device_->createFenceUnique(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
	}

	memory_properties_= physical_device.getMemoryProperties();
}

SystemWindow::~SystemWindow()
{
	// Sync before destruction.
	vk_device_->waitIdle();
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

vk::CommandBuffer SystemWindow::BeginFrame()
{
	current_frame_data_= &frames_data_[frame_count_ % frames_data_.size()];
	++frame_count_;

	vk_device_->waitForFences(
		1u, &*current_frame_data_->submit_fence,
		VK_TRUE,
		std::numeric_limits<uint64_t>::max());

	vk_device_->resetFences(1u, &*current_frame_data_->submit_fence);

	current_swapchain_image_index_=
		vk_device_->acquireNextImageKHR(
			*vk_swapchain_,
			std::numeric_limits<uint64_t>::max(),
			*current_frame_data_->image_available_semaphore,
			vk::Fence()).value;

	const vk::CommandBuffer command_buffer= *current_frame_data_->command_buffer;

	command_buffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	ClearScreen(command_buffer);

	return command_buffer;
}

void SystemWindow::EndFrame()
{
	current_frame_data_->command_buffer->end();

	const vk::PipelineStageFlags wait_dst_stage_mask= vk::PipelineStageFlagBits::eColorAttachmentOutput;
	const vk::SubmitInfo vk_submit_info(
		1u, &*current_frame_data_->image_available_semaphore,
		&wait_dst_stage_mask,
		1u, &*current_frame_data_->command_buffer,
		1u, &*current_frame_data_->rendering_finished_semaphore);
	vk_queue_.submit(vk::ArrayProxy<const vk::SubmitInfo>(vk_submit_info), *current_frame_data_->submit_fence);

	vk_queue_.presentKHR(
		vk::PresentInfoKHR(
			1u, &*current_frame_data_->rendering_finished_semaphore,
			1u, &*vk_swapchain_,
			&current_swapchain_image_index_,
			nullptr));
}

void SystemWindow::ClearScreen(const vk::CommandBuffer command_buffer)
{
	const vk::ImageMemoryBarrier vk_image_memory_barrier(
		vk::AccessFlagBits::eTransferWrite,
		vk::AccessFlagBits::eMemoryRead,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::ePresentSrcKHR,
		vk_queue_familiy_index_,
		vk_queue_familiy_index_,
		vk_swapchain_images_[current_swapchain_image_index_],
		vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u));

	command_buffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eBottomOfPipe,
			vk::DependencyFlags(),
			0u, nullptr,
			0u, nullptr,
			1u, &vk_image_memory_barrier);
}

vk::Device SystemWindow::GetVulkanDevice() const
{
	return *vk_device_;
}

vk::Format SystemWindow::GetSurfaceFormat() const
{
	return swapchain_image_format_;
}

vk::Extent2D SystemWindow::GetViewportSize() const
{
	return viewport_size_;
}

const vk::PhysicalDeviceMemoryProperties& SystemWindow::GetMemoryProperties() const
{
	return memory_properties_;
}

const std::vector<vk::UniqueImageView>& SystemWindow::GetSwapchainImagesViews() const
{
	return vk_swapchain_images_view_;
}

size_t SystemWindow::GetCurrentSwapchainImageIndex() const
{
	return current_swapchain_image_index_;
}

} // namespace KK
