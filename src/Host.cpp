#include "Host.hpp"
#include <thread>


namespace KK
{

Host::Host()
	: system_window_(SystemWindow::GAPISupport::Vulkan)
	, window_vulkan_(system_window_)
	, world_renderer_(window_vulkan_)
	, text_out_(window_vulkan_)
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

	const uint8_t color[4]={ 255, 255, 255, 255 };
	text_out_.AddText( 0.0f, 0.0f, 0.25f, color, "lorem ipsum\n one 66 ! {}" );

	const auto command_buffer= window_vulkan_.BeginFrame();

	world_renderer_.BeginFrame(command_buffer);
	text_out_.BeginFrame(command_buffer);

	window_vulkan_.EndFrame(
		{
			[&](const vk::CommandBuffer command_buffer)
			{
				world_renderer_.EndFrame(
					command_buffer,
					float(std::chrono::duration_cast<std::chrono::milliseconds>(tick_start_time - init_time_).count()) / 1000.0f);
			},
			[&](const vk::CommandBuffer command_buffer)
			{
				text_out_.EndFrame(command_buffer);
			},
		});

	const Clock::time_point tick_end_time= Clock::now();
	const auto frame_dt= tick_end_time - tick_start_time;
	const std::chrono::milliseconds min_frame_duration(5);
	if(frame_dt <= min_frame_duration)
	{
		std::this_thread::sleep_for(min_frame_duration - frame_dt);
	}

	return false;
}

} // namespace KK
