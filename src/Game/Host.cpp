#include "Host.hpp"
#include <thread>


namespace KK
{

namespace
{

float CalculateAspect(const vk::Extent2D& viewport_size)
{
	return float(viewport_size.width) / float(viewport_size.height);
}

} // namespace

Host::Host()
	: settings_("kk_config.cfg")
	, commands_processor_(settings_)
	, ticks_counter_(std::chrono::milliseconds(500))
	, system_window_(settings_, SystemWindow::GAPISupport::Vulkan)
	, window_vulkan_(system_window_)
	, gpu_data_uploader_(window_vulkan_)
	, world_renderer_(window_vulkan_, gpu_data_uploader_, GenerateWorld())
	, text_out_(window_vulkan_, gpu_data_uploader_)
	, camera_controller_(CalculateAspect(window_vulkan_.GetViewportSize()))
	, init_time_(Clock::now())
	, prev_tick_time_(init_time_)
{
}

bool Host::Loop()
{
	const Clock::time_point tick_start_time= Clock::now();
	const auto dt= tick_start_time - prev_tick_time_;
	prev_tick_time_ = tick_start_time;

	const float dt_s= float(dt.count()) * float(Clock::duration::period::num) / float(Clock::duration::period::den);

	for(const SystemEvent& system_event : system_window_.ProcessEvents())
	{
		if(std::get_if<SystemEventTypes::QuitEvent>(&system_event) != nullptr)
			return true;
	}

	camera_controller_.Update(dt_s, system_window_.GetInputState());

	ticks_counter_.Tick();
	{
		const int freq_i= int(std::round(std::min(999.0f, ticks_counter_.GetTicksFrequency())));

		char fps_str[]= "fps 000";
		fps_str[4]= freq_i >= 100 ? char('0' + freq_i / 100 % 10) : ' ';
		fps_str[5]= freq_i >=  10 ? char('0' + freq_i /  10 % 10) : ' ';
		fps_str[6]= char('0' + freq_i /   1 % 10);
		const uint8_t color[4]={ 255, 255, 255, 255 };
		text_out_.AddText(0.0f, 0.0f, 0.25f, color, fps_str);
	}

	const auto command_buffer= window_vulkan_.BeginFrame();

	world_renderer_.BeginFrame(command_buffer, camera_controller_.CalculateViewMatrix());
	text_out_.BeginFrame(command_buffer);

	window_vulkan_.EndFrame(
		{
			[&](const vk::CommandBuffer command_buffer)
			{
				world_renderer_.EndFrame(command_buffer);
			},
			[&](const vk::CommandBuffer command_buffer)
			{
				text_out_.EndFrame(command_buffer);
			},
		});

	const Clock::time_point tick_end_time= Clock::now();
	const auto frame_dt= tick_end_time - tick_start_time;
	const std::chrono::milliseconds min_frame_duration(10);
	if(frame_dt <= min_frame_duration)
	{
		std::this_thread::sleep_for(min_frame_duration - frame_dt);
	}

	return false;
}

} // namespace KK
