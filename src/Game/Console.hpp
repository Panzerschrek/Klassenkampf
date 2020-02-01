#pragma once
#include "CommandsProcessor.hpp"
#include "Log.hpp"
#include "SystemEvent.hpp"
#include "TextOut.hpp"
#include <chrono>
#include <list>
#include <string>


namespace KK
{

class Console final
{
public:
	Console(CommandsProcessor& commands_processor, TextOut& text_out);
	~Console();

	void Toggle();
	bool IsActive() const;

	void ProcessEvents(const SystemEvents& events);
	void Draw();

private:
	using Clock= std::chrono::steady_clock;
	using Time= Clock::time_point;

	struct UserMessage
	{
		std::string text;
		Time time;
	};

private:
	void RemoveOldUserMessages(Time current_time);
	void DrawUserMessages();

	void LogCallback(std::string str, Log::LogLevel log_level);

	void WriteHistory();
	void CopyLineFromHistory();

private:
	static constexpr size_t c_max_input_line_length= 64u;
	static constexpr size_t c_max_lines= 128u;
	static constexpr size_t c_max_history= 64u;

	CommandsProcessor& commands_processor_;
	TextOut& text_out_;

	float position_= 0.0f;
	float current_speed_= -1.0f;
	Time last_draw_time_;

	std::list<std::string> lines_;
	size_t lines_position_= 0u;

	// TODO - Maybe use raw array or std::array, instead std::string?
	std::string history_[ c_max_history ];
	size_t history_size_= 0u;
	size_t next_history_line_index_= 0u;
	size_t current_history_line_= 0u; // From newer to older. 0 - current line without history.

	char input_line_[ c_max_input_line_length + 1u ];
	size_t input_cursor_pos_= 0u;

	std::list<UserMessage> user_messages_;
};

} // namespace KK
