#ifndef testCircuitCommand_h
#define testCircuitCommand_h

#include "../command.h"

class TestCircuitCommand : public Command {
public:
	TestCircuitCommand() : Command("test_circuit") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Tests a circuit."; }
};

#endif /* testCircuitCommand_h */
