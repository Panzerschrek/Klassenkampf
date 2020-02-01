#include "CommandsProcessor.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>


namespace KK
{

CommandsProcessor::CommandsProcessor(Settings& settings)
	: settings_(settings)
{}

void CommandsProcessor::RegisterCommands(CommandsMapConstWeakPtr commands)
{
	commands_maps_.emplace_back(std::move(commands));
}

void CommandsProcessor::ProcessCommand(const std::string_view command_string)
{
	const CommandParsed command_parsed= ParseCommad(command_string);
	if(command_parsed.first.empty())
		return;

	for(size_t m= 0u; m < commands_maps_.size();)
	{
		const CommandsMapConstPtr commads_map= commands_maps_[m].lock();
		if(commads_map != nullptr)
		{
			const auto it= commads_map->find(command_parsed.first);
			if(it != commads_map->end())
			{
				// Found it
				it->second(command_parsed.second);
				return;
			}

			m++;
		}
		else
		{
			// Remove expired weak pointers.
			if(m != commands_maps_.size() - 1u)
				commands_maps_[m]= commands_maps_.back();
			commands_maps_.pop_back();
		}
	}

	// Search settins variable.
	if( settings_.HasValue(command_parsed.first))
	{
		if( command_parsed.second.empty() )
		{
			// TODO - print into log
		}
		else
			settings_.SetString(command_parsed.first, command_parsed.second.front());
	}
	else
	{
		// TODO
		//Log::Info( command_parsed.first, ": command not found" );
	}
}

std::string CommandsProcessor::TryCompleteCommand(const std::string_view command_string) const
{
	const std::string command= ParseCommad(command_string).first;

	std::vector<std::string> candidates;

	// Get matching commands.
	for(const CommandsMapConstWeakPtr& map_weak_ptr : commands_maps_)
	{
		const CommandsMapConstPtr commads_map= map_weak_ptr.lock();
		if(commads_map != nullptr)
		{
			for( const CommandsMap::value_type& command_value : *commads_map )
			{
				const char* const str= command_value.first.c_str();
				if(std::strstr(str, command.c_str()) == str)
					candidates.push_back( command_value.first );
			}
		}
	}

	// Get matching settings variables.
	const std::vector<std::string> settings_candidates=
		settings_.GetSettingsKeysStartsWith(command);
	candidates.insert(settings_candidates.end(), settings_candidates.begin(), settings_candidates.end());

	if( candidates.size() == 0u )
		return command;

	// Print candidates.
	if(candidates.size() > 1u)
	{
		// TODO - write to log.
		//Log::Info( ">", command );
		std::sort( candidates.begin(), candidates.end() );
		//for(const std::string& candidate : candidates)
			//Log::Info( "  ", candidate );
	}

	// Find common prefix for candidates.
	size_t pos= command.size();
	while(true)
	{
		if(pos >= candidates[0].size())
			break;

		bool equals= true;
		for(const std::string& candidate : candidates)
			if(pos >= candidate.size() || candidate[pos] != candidates[0][pos])
			{
				equals= false;
				break;
			}

		if(!equals)
			break;
		++pos;
	}

	return std::string(candidates[0].begin(), candidates[0].begin() + pos);
}

CommandsProcessor::CommandParsed CommandsProcessor::ParseCommad(const std::string_view command_string)
{
	std::pair< std::string, CommandsArguments > result;

	const char* s= command_string.data();
	const char* const s_end= s + command_string.size();

	// Skip trailing spaces
	while(s < s_end && std::isspace(*s))
		++s;

	// Process command name
	while(s < s_end && !std::isspace(*s))
	{
		result.first.push_back(char(std::tolower(*s)));
		++s;
	}

	while(s < s_end)
	{
		// Skip trailing spaces
		while(s < s_end && std::isspace(*s))
			++s;

		// Do not create argument if string is over.
		if(s == s_end)
			break;

		result.second.emplace_back();

		// Process argument
		if(*s == '"') // Arg in quotes
		{
			++s;
			while(s < s_end && *s != '"')
			{
				if(*s == '\\' && s + 1 < s_end && s[1] == '"')
					++s;// Escaped quote
				result.second.back().push_back(*s);
				++s;
			}
			if(*s == '"')
				++s;
			else
				break;
		}
		else // Space-separated argument
		{
			while(s < s_end && !std::isspace(*s))
			{
				result.second.back().push_back(*s);
				++s;
			}
		}
	}

	return result;
}

} // namespace KK
