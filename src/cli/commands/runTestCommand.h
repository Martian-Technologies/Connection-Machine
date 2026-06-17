#ifndef runTestCommand_h
#define runTestCommand_h

#include "../command.h"

class RunTestCommand : public Command {
public:
	RunTestCommand() : Command("run_test") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Runs a test on a specified circuit"; }
};

#endif /* runTestCommand_h */
