#include <gtest/gtest.h>

#include "backend/evaluator/simulator/gateGroup.h"
#include "backend/evaluator/simulator/logicGroupRunner.h"

TEST(LogicGroupRunnerTest, RemovedGroupInvalidatesOldGateMapping) {
	LogicGroupRunner runner;

	const eval_gate_id gateId(42);
	const gate_group_id_t groupId(0);
	const EvalConnectionPoint outputConnectionPoint { gateId, connection_end_id_t(0) };
	const LinkedGateGroup group(
		{ SimulatorGate { gateId, BlockType::CONSTANT_ON, {} } },
		{},
		{}
	);

	runner.setGroups({ { groupId, group } });

	const auto simulatorStateIndex = runner.getSimulatorStateIndex(outputConnectionPoint);
	EXPECT_NE(simulatorStateIndex, simulator_state_reference(3));

	runner.setGroups({});

	EXPECT_EQ(runner.getState(simulatorStateIndex), logic_state_t::UNDEFINED);
}
