#ifndef openCommand_h
#define openCommand_h

#include "../command.h"

class OpenCommand : public Command {
public:
	OpenCommand() : Command("open") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
};

#endif /* openCommand_h */
