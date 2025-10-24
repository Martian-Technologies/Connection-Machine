#ifndef helpCommand_h
#define helpCommand_h

#include "../command.h"

class HelpCommand : public Command {
public:
	HelpCommand() : Command("help") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Brings up this menu."; }
};

#endif /* helpCommand_h */