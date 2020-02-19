#pragma once
#include "CameraController.hpp"
#include "CommandsProcessor.hpp"
#include "Console.hpp"
#include "GPUDataUploader.hpp"
#include "Settings.hpp"
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
	void CommandQuit();

private:
	using Clock= std::chrono::steady_clock;

	Settings settings_;
	CommandsProcessor commands_processor_;
	TicksCounter ticks_counter_;
	SystemWindow system_window_;
	WindowVulkan window_vulkan_;
	GPUDataUploader gpu_data_uploader_;
	TextOut text_out_;
	Console console_;
	CameraController camera_controller_;
	WorldRenderer world_renderer_;
	CommandsMapPtr commands_map_;

	const Clock::time_point init_time_;
	Clock::time_point prev_tick_time_;

	bool quit_requested_= false;
};

} // namespace KK
