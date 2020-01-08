#include "Host.hpp"
#include <thread>


namespace KK
{

Host::Host()
	: system_window_()
	, world_renderer_(
		system_window_.GetVulkanDevice(),
		system_window_.GetSurfaceFormat(),
		system_window_.GetViewportSize(),
		system_window_.GetSwapchainImagesViews())
	, init_time_(Clock::now())
	, prev_tick_time_(init_time_)
{
}

bool Host::Loop()
{
	const Clock::time_point tick_start_time= Clock::now();
	const auto dt= tick_start_time - prev_tick_time_;
	prev_tick_time_ = tick_start_time;

	for(const SystemEvent& system_event : system_window_.ProcessEvents())
	{
		if(std::get_if<SystemEventTypes::QuitEvent>(&system_event) != nullptr)
			return true;

	}

	const auto command_buffer= system_window_.BeginFrame();
	world_renderer_.Draw(
		command_buffer,
		system_window_.GetCurrentSwapchainImageIndex(),
		float(std::chrono::duration_cast<std::chrono::milliseconds>(tick_start_time - init_time_).count()) / 1000.0f);
	system_window_.EndFrame();

	const Clock::time_point tick_end_time= Clock::now();
	const auto frame_dt= tick_start_time - tick_end_time;
	const std::chrono::milliseconds min_frame_duration(5);
	if(frame_dt <= min_frame_duration)
	{
		std::this_thread::sleep_for(min_frame_duration - frame_dt);
	}

	return false;
}

} // namespace KK
