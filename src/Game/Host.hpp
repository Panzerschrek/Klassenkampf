#pragma once
#include "CameraController.hpp"
#include "SystemWindow.hpp"
#include "TextOut.hpp"
#include "TicksCounter.hpp"
#include "WindowVulkan.hpp"
#include "WorldRenderer.hpp"
#include <chrono>


namespace KK
{

class Host final
{
public:
	Host();

	// Returns false on quit
	bool Loop();

private:
	using Clock= std::chrono::steady_clock;

	TicksCounter ticks_counter_;
	SystemWindow system_window_;
	WindowVulkan window_vulkan_;
	WorldRenderer world_renderer_;
	TextOut text_out_;
	CameraController camera_controller_;

	const Clock::time_point init_time_;
	Clock::time_point prev_tick_time_;
};

} // namespace KK
