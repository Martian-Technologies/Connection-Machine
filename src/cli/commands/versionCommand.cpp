#include "versionCommand.h"

#include "environment/environment.h"
#include "../commandManager.h"
#include "util/runAtStartup.h"
#include "util/version.h"

runAtStartup(CommandManager::get().registerCommand(std::make_unique<VersionCommand>());)

void VersionCommand::run(const std::vector<std::string>& args, Environment& environment) {
	if (args.size() != 1) {
		logError("Wrong number of arguments passed to open. Should be none.", "VersionCommand");
		return;
	}
	logInfo("Connection Machine Version: {}", "", getCurrentVersion().toString());
}
