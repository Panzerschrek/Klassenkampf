#pragma once
#include "SystemWindow.hpp"
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
	Clock::time_point prev_tick_time_;
};

} // namespace KK
