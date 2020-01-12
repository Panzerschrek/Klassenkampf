#pragma once
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
	explicit SystemWindow(GAPISupport gapi_support);
	~SystemWindow();

	SystemEvents ProcessEvents();

	SDL_Window* GetSDLWindow() const;

private:
	SDL_Window* window_= nullptr;
};

} // namespace KK
