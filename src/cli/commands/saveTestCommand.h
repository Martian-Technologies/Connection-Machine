#ifndef saveTestCommand_h
#define saveTestCommand_h

#include "../command.h"

class SaveTestCommand : public Command {
public:
	SaveTestCommand() : Command("save_test") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Saves a test to the specified file"; }
};

#endif /* saveTestCommand_h */
