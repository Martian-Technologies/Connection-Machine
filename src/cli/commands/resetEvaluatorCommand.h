/*#ifndef resetSimulatorCommand_h
#define resetSimulatorCommand_h

#include "../command.h"

class ResetSimulatorCommand : public Command {
public:
	ResetSimulatorCommand() : Command("reset_simulator") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Resets the simulator of specified ID."; }
};

#endif /* resetSimulatorCommand_h */
