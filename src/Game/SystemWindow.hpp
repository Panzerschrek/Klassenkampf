#pragma once
#include "Settings.hpp"
#include "SystemEvent.hpp"
#include <SDL_video.h>


namespace KK
{

class SystemWindow final
{
public:
	enum class GAPISupport
	{
		Vulkan,
	};

public:
	SystemWindow(Settings& settings, GAPISupport gapi_support);
	~SystemWindow();

	SystemEvents ProcessEvents();

	InputState GetInputState();

	SDL_Window* GetSDLWindow() const;

private:
	SDL_Window* window_= nullptr;
};

} // namespace KK
