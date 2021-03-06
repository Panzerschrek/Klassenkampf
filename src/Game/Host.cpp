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
	, window_vulkan_(system_window_, settings_)
	, gpu_data_uploader_(window_vulkan_)
	, text_out_(window_vulkan_, gpu_data_uploader_)
	, console_(commands_processor_, text_out_)
	, camera_controller_(settings_, CalculateAspect(window_vulkan_.GetViewportSize()))
	, world_renderer_(settings_, commands_processor_, window_vulkan_, gpu_data_uploader_, camera_controller_, GenerateWorld())
	, init_time_(Clock::now())
	, prev_tick_time_(init_time_)
{
	commands_map_=
		std::make_shared<CommandsMap>(
			CommandsMap(
				{
					{"quit", std::bind(&Host::CommandQuit, this)},
				}));

	commands_processor_.RegisterCommands(commands_map_);
}

bool Host::Loop()
{
	const Clock::time_point tick_start_time= Clock::now();
	const auto dt= tick_start_time - prev_tick_time_;
	prev_tick_time_ = tick_start_time;

	const float dt_s= float(dt.count()) * float(Clock::duration::period::num) / float(Clock::duration::period::den);

	const SystemEvents system_events= system_window_.ProcessEvents();
	for(const SystemEvent& system_event : system_events)
	{
		if(std::get_if<SystemEventTypes::QuitEvent>(&system_event) != nullptr)
			return true;

		if(const auto* const key_event= std::get_if<SystemEventTypes::KeyEvent>(&system_event))
		{
			if(key_event->pressed && key_event->key_code == SystemEventTypes::KeyCode::BackQuote)
				console_.Toggle();
		}
	}
	if(console_.IsActive())
		console_.ProcessEvents(system_events);
	else
		camera_controller_.Update(dt_s, system_window_.GetInputState());

	ticks_counter_.Tick();
	{
		const int freq_i= int(std::round(std::min(999.0f, ticks_counter_.GetTicksFrequency())));

		char fps_str[]= "fps 000";
		fps_str[4]= freq_i >= 100 ? char('0' + freq_i / 100 % 10) : ' ';
		fps_str[5]= freq_i >=  10 ? char('0' + freq_i /  10 % 10) : ' ';
		fps_str[6]= char('0' + freq_i /   1 % 10);
		const uint8_t color[4]={ 255, 255, 255, 255 };
		const float scale= 0.25f;
		text_out_.AddText(
			text_out_.GetMaxColumns() / scale - float(sizeof(fps_str)),
			0.0f,
			scale,
			color,
			fps_str);
	}

	console_.Draw();

	const auto command_buffer= window_vulkan_.BeginFrame();

	world_renderer_.BeginFrame(command_buffer);
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

	const Settings::RealType max_fps= std::max(15.0, std::min(settings_.GetOrSetReal("cl_max_fps", 120.0), 480.0));

	const std::chrono::milliseconds min_frame_duration(uint32_t(1000.0f / max_fps));
	if(frame_dt <= min_frame_duration)
	{
		std::this_thread::sleep_for(min_frame_duration - frame_dt);
	}

	return quit_requested_;
}

void Host::CommandQuit()
{
	quit_requested_= true;
}

} // namespace KK
