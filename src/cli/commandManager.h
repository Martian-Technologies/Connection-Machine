#ifndef commandManager_h
#define commandManager_h

#include "command.h"
class Environment;

class CommandManager {
// contains and runs all the commands that can be run from the CLI app
public:
	static CommandManager& get();
	static void kill();

    void registerCommand(std::unique_ptr<Command>&& command);
    const std::map<std::string, std::unique_ptr<Command>>& getCommandMap() const { return commandMap; }
    void run(const std::vector<std::string>& commandParams, Environment& environment);
private:
    std::map<std::string, std::unique_ptr<Command>> commandMap;
};

#endif /* commandManager_h */