#include "Assert.hpp"
#include "SystemWindow.hpp"
#include <SDL.h>
#include <SDL_vulkan.h>
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
	vk_create_info.enabledLayerCount= 0;
	vk_create_info.ppEnabledLayerNames= nullptr;
	vk_create_info.enabledExtensionCount= extension_names_count;
	vk_create_info.ppEnabledExtensionNames= extensions_list.data();

	if(vkCreateInstance(&vk_create_info, nullptr, &vk_instance_) != VK_SUCCESS)
	{
		std::exit(-1);
	}

	if(!SDL_Vulkan_CreateSurface(window_, vk_instance_, &vk_surface_))
	{
		std::exit(-1);
	}

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

	const float queue_priorities[1]{0.0f};
	VkDeviceQueueCreateInfo vk_device_queue_create_info;
	std::memset(&vk_device_queue_create_info, 0, sizeof(vk_device_queue_create_info));
	vk_device_queue_create_info.sType= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	vk_device_queue_create_info.pNext= nullptr;
	vk_device_queue_create_info.flags= 0u;
	vk_device_queue_create_info.queueFamilyIndex= queue_family_index;
	vk_device_queue_create_info.queueCount= 1u;
	vk_device_queue_create_info.pQueuePriorities= queue_priorities;

	const char* const device_extension_names[]{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkDeviceCreateInfo vk_device_create_info;
	std::memset(&vk_device_create_info, 0, sizeof(vk_device_create_info));
	vk_device_create_info.sType= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	vk_device_create_info.queueCreateInfoCount= 1u;
	vk_device_create_info.pQueueCreateInfos= &vk_device_queue_create_info;
	vk_device_create_info.enabledLayerCount= 0u;
	vk_device_create_info.ppEnabledLayerNames= nullptr;
	vk_device_create_info.enabledExtensionCount= std::size(device_extension_names);
	vk_device_create_info.ppEnabledExtensionNames= device_extension_names;
	vk_device_create_info.pEnabledFeatures= nullptr;

	if(vkCreateDevice(physical_device, &vk_device_create_info, nullptr, &vk_device_) != VK_SUCCESS)
	{
		std::exit(-1);
	}

	vkGetDeviceQueue(vk_device_, queue_family_index, 0u, &vk_queue_);

	return;
}

SystemWindow::~SystemWindow()
{
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

void SystemWindow::BeginFrame()
{
}

void SystemWindow::EndFrame()
{
	SDL_UpdateWindowSurface(window_);
}

} // namespace KK
