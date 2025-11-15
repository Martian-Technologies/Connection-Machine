#include "commandManager.h"

#include "environment/environment.h"

std::optional<CommandManager> commandManagerSingleton;

CommandManager& CommandManager::get() {
	if (!commandManagerSingleton) commandManagerSingleton.emplace();
	return *commandManagerSingleton;
}

void CommandManager::kill() {
	commandManagerSingleton.reset();
}

void CommandManager::registerCommand(std::unique_ptr<Command>&& command) {
    auto pair = commandMap.try_emplace(command->getName(), std::move(command));
    if (!pair.second) {
        logError("Failed to register command {}: duplicate command name found", "CommandManager", command->getName());
    }
}

void CommandManager::run(const std::vector<std::string>& commandArgs, Environment& environment) {
    auto iter = commandMap.find(commandArgs[0]);
    if (iter == commandMap.end()) {
		logError("Failed to run nonexistent command {}. Use the \"help\" command for a list of commands.", "CommandManager", commandArgs[0]);
		return;
    }
    iter->second->run(commandArgs, environment);
}
