#include "commandManager.h"

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
        logError("Failed to add command {}: duplicate command name found", "CommandManager", command->getName());
    }
}

void CommandManager::run(const std::vector<std::string>& commandParams, Environment& environment) {
    auto iter = commandMap.find(commandParams[0]);
    if (iter == commandMap.end()) {
		logError("Failed to run nonexistent command {}", "CommandManager", commandParams[0]);
		return;
    }
    iter->second->run(commandParams);
}
