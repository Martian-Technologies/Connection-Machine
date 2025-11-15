#ifndef listCircuitsCommand_h
#define listCircuitsCommand_h

#include "../command.h"

class ListCircuitsCommand : public Command {
public:
	ListCircuitsCommand() : Command("list_circuits") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Lists all open circuits."; }
};

#endif /* listCircuitsCommand_h */
