#pragma once
#include "SystemWindow.hpp"
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

	SystemWindow system_window_;
	WorldRenderer world_renderer_;

	const Clock::time_point init_time_;
	Clock::time_point prev_tick_time_;
};

} // namespace KK
