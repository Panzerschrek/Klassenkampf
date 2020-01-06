#pragma once
#include "SystemEvent.hpp"
#include <SDL_video.h>


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
};

} // namespace KK
