#include "helpCommand.h"

#include "environment/environment.h"
#include "../commandManager.h"
#include "util/runAtStartup.h"

runAtStartup(CommandManager::get().registerCommand(std::make_unique<HelpCommand>());)

void HelpCommand::run(const std::vector<std::string>& args, Environment& environment) {
	logInfo(args[0]);
	const auto& commandMap = CommandManager::get().getCommandMap();
    std::string helpString = "";
    for (auto iter = commandMap.begin(); iter != commandMap.end(); ++iter) {
        helpString = helpString + iter->second->getName() + " - " + iter->second->getHelpString() + "\n";
    }
	std::cout << helpString;
}