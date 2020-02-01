#include "Console.hpp"
#include "Assert.hpp"
#include <algorithm>
#include <cstring>


namespace KK
{

namespace
{

const float g_user_message_show_time_s= 5.0f;
const size_t g_max_user_messages= 4u;

} // namespace

Console::Console(CommandsProcessor& commands_processor, TextOut& text_out)
	: commands_processor_(commands_processor)
	, text_out_(text_out)
	, last_draw_time_(Clock::now())
{
	input_line_[0]= '\0';

	Log::SetLogCallback(std::bind(&Console::LogCallback, this, std::placeholders::_1, std::placeholders::_2));
}

Console::~Console()
{
	Log::SetLogCallback(nullptr);
}

void Console::Toggle()
{
	current_speed_= current_speed_ > 0.0f ? -1.0f : 1.0f;
}

bool Console::IsActive() const
{
	return current_speed_ > 0.0f;
}

void Console::ProcessEvents(const SystemEvents& events)
{
	using KeyCode= SystemEventTypes::KeyCode;

	for(const SystemEvent& event : events)
	{
		if(const auto* char_input= std::get_if<SystemEventTypes::CharInputEvent>(&event))
		{
			if(char_input->ch != '`' && input_cursor_pos_ < c_max_input_line_length)
			{
				input_line_[ input_cursor_pos_ ]= char_input->ch;
				++input_cursor_pos_;
				input_line_[ input_cursor_pos_ ]= '\0';
			}
		}
		if(const auto* key_event= std::get_if<SystemEventTypes::KeyEvent>(&event))
		{
			if(!key_event->pressed)
				continue;

			const KeyCode key_code= key_event->key_code;
			if(key_code == KeyCode::Enter)
			{
				lines_position_= 0u;
				Log::Info(input_line_);

				if(input_cursor_pos_ > 0u)
				{
					WriteHistory();
					commands_processor_.ProcessCommand(input_line_);
				}

				input_cursor_pos_= 0u;
				input_line_[ input_cursor_pos_ ]= '\0';
			}
			else if(key_code == KeyCode::Escape)
				Toggle();
			else if(key_code == KeyCode::Backspace)
			{
				if(input_cursor_pos_ > 0u)
				{
					--input_cursor_pos_;
					input_line_[ input_cursor_pos_ ]= '\0';
				}
			}
			else if(key_code == KeyCode::Tab)
			{
				const std::string completed_command= commands_processor_.TryCompleteCommand(input_line_);
				std::strncpy(input_line_, completed_command.c_str(), sizeof(input_line_));
				input_cursor_pos_= std::strlen(input_line_);
			}
			else if(key_code == KeyCode::Up)
			{
				if(history_size_ > 0u)
				{
					if(current_history_line_ < history_size_ && current_history_line_ < c_max_history)
						++current_history_line_;
					CopyLineFromHistory();
				}
			}
			else if(key_code == KeyCode::Down)
			{
				if(current_history_line_ > 0u)
				{
					--current_history_line_;

					if(current_history_line_ > 0u && history_size_ > 0u)
						CopyLineFromHistory();
				}
				if(current_history_line_ == 0u)
				{
					// Reset current line
					input_line_[0]= '\0';
					input_cursor_pos_= 0;
				}
			}
			else if(key_code == KeyCode::PageUp)
			{
				lines_position_+= 3u;
				lines_position_= std::min(lines_position_, lines_.size());
			}
			else if(key_code == KeyCode::PageDown)
			{
				if(lines_position_ > 3u)
					lines_position_-= 3u;
				else
					lines_position_= 0u;
			}
		}

	} // for events
}

void Console::Draw()
{
	const Time current_time= Clock::now();
	const float time_delta_s= float(std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_draw_time_).count()) / 1000.0f;
	last_draw_time_= current_time;

	position_+= current_speed_ * time_delta_s;
	position_= std::min(1.0f, std::max(0.0f, position_));

	RemoveOldUserMessages(current_time);
	if(position_ <= 0.0f)
	{
		DrawUserMessages();
		return;
	}

	const float scale= 1.0f / 3.0f;
	const float offset= 0.0f;
	const uint8_t color[4]{ 255, 255, 240, 255 };

	float y= text_out_.GetMaxRows() / scale * 0.5f * position_;

	const int cursor_period_ms= 500;
	const bool draw_cursor= std::chrono::duration_cast<std::chrono::milliseconds>(current_time - Time()).count() % cursor_period_ms < cursor_period_ms / 2;

	char line_with_cursor[ c_max_input_line_length + 3u ];
	std::snprintf(line_with_cursor, sizeof(line_with_cursor), draw_cursor ? "> %s_" : "> %s", input_line_);

	text_out_.AddText(offset, y, scale, color, line_with_cursor);
	y-= 1.5f;

	if(lines_position_ > 0u)
	{
		const char* const str=
		"  ^    ^    ^    ^    ^    ^    ^    ^    ^    ^    ^    ^    ^    ^    ^    ^    ^    ^    ^    ^    ^    ^    ^    ^  ";
		text_out_.AddText(offset, y, scale, color, str);
		y-= 1.0f;
	}

	for(
		auto it= std::next(lines_.rbegin(), std::min(lines_position_, lines_.size()));
		it != lines_.rend() && y > -1.0f;
		++it, y-= 1.0f)
	{
		text_out_.AddText(offset, y, scale, color, *it);
	}
}

void Console::RemoveOldUserMessages(const Time current_time)
{
	while(user_messages_.size() > g_max_user_messages)
		user_messages_.pop_front();

	while(! user_messages_.empty() &&
		float(std::chrono::duration_cast<std::chrono::milliseconds>(current_time - user_messages_.front().time).count()) / 1000.0f > g_user_message_show_time_s)
		user_messages_.pop_front();
}

void Console::DrawUserMessages()
{
	const float scale= 1.0f;
	const uint8_t color[4]{ 240, 240, 255, 255 };

	float y= 0.0f;
	for(const UserMessage& message : user_messages_)
	{
		text_out_.AddText(0.0f, y, scale, color, message.text);
		y+= 1.0f;
	}
}

void Console::LogCallback(std::string str, const Log::LogLevel log_level)
{
	lines_.emplace_back(std::move(str));

	if(lines_.size() == c_max_lines)
		lines_.pop_front();

	lines_position_= 0u;

	if(log_level == Log::LogLevel::User)
	{
		user_messages_.emplace_back();
		user_messages_.back().text= lines_.back();
		user_messages_.back().time= Clock::now();
	}
}

void Console::WriteHistory()
{
	history_[ next_history_line_index_ ]= input_line_;
	next_history_line_index_= (next_history_line_index_ + 1u) % c_max_history;
	++history_size_;
	current_history_line_= 0u;
}

void Console::CopyLineFromHistory()
{
	KK_ASSERT(history_size_ > 0u);

	std::strncpy(
		input_line_,
		history_[ (history_size_ - current_history_line_) % c_max_history ].c_str(),
		sizeof(input_line_));

	input_cursor_pos_= std::strlen(input_line_);
}

} // namespace KK
