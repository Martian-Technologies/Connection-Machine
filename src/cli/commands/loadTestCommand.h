#ifndef loadTestCommand_h
#define loadTestCommand_h

#include "../command.h"

class LoadTestCommand : public Command {
public:
	LoadTestCommand() : Command("load_test") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Loads a circuit test group."; }
};

#endif /* loadTestCommand_h */
