#pragma once
#include "Settings.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace KK
{

using CommandsArguments= std::vector<std::string>;

using CommandFunction= std::function<void(const CommandsArguments&)>;
using CommandsMap= std::unordered_map< std::string, CommandFunction >;
using CommandsMapPtr= std::shared_ptr<CommandsMap>;
using CommandsMapConstPtr= std::shared_ptr<const CommandsMap>;
using CommandsMapConstWeakPtr= std::weak_ptr<const CommandsMap>;

class CommandsProcessor final
{
public:
	explicit CommandsProcessor(Settings& settings);

	// Register commands.
	// Commands owner must store pointer to commands map.
	void RegisterCommands(CommandsMapConstWeakPtr commands);

	void ProcessCommand(std::string_view command_string);

	// Retruns completed string, prints to log candidates commands.
	std::string TryCompleteCommand(std::string_view command_string) const;

private:
	using CommandParsed= std::pair< std::string, CommandsArguments >;
	// Extract command from string, convert commnad to lower case, extract arguments
	static CommandParsed ParseCommad(std::string_view command_string);

private:
	std::vector< CommandsMapConstWeakPtr > commands_maps_;
	Settings& settings_;
};

} // namespace KK
