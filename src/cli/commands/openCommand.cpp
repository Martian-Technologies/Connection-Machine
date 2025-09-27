#include "openCommand.h"

#include "environment/environment.h"
#include "../commandManager.h"
#include "util/runAtStartup.h"

runAtStartup(CommandManager::get().registerCommand(std::make_unique<OpenCommand>());)

void OpenCommand::run(const std::vector<std::string>& args, Environment& environment) {
	if (args.size() != 2) {
		logError("Wrong number of arguments passed to open. Should be 'open \"path\"'.", "OpenCommand");
		return;
	}
	logInfo(args[1]);
	environment.getCircuitFileManager().loadFromFile(args[1]);
}
