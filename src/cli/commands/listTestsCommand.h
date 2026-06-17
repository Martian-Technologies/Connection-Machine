#ifndef listTestsCommand_h
#define listTestsCommand_h

#include "../command.h"

class ListTestsCommand : public Command {
public:
	ListTestsCommand() : Command("list_tests") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Lists all open tests."; }
};

#endif /* listTestsCommand_h */
