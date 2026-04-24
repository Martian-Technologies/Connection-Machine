#include <gtest/gtest.h>
#include "environment/environment.h"
#include "backend/evaluator/simulator/evalLogicSimulator.h"
#include "util/algorithm.h"

class PrimitivesEvaluatorTest : public ::testing::Test {
protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment { false };
	EvalLogicSimulator* simulator = nullptr;
	SharedCircuit circuit = nullptr;
};

void PrimitivesEvaluatorTest::SetUp() {
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	circuit = environment.getBackend().getCircuit(circuitId);
	simulator_id_t simulatorId = environment.getBackend().createSimulator(circuitId).value();
	simulator = environment.getBackend().getSimulator(simulatorId);
}

void PrimitivesEvaluatorTest::TearDown() {
	circuit.reset();
	simulator = nullptr;
}

namespace {

	logic_state_t naiveButCorrectGateImplementation(BlockType blockType, std::vector<logic_state_t> inputs) {
		int numLow = 0;
		int numHigh = 0;
		int numFloating = 0;
		int numUndefined = 0;
		for (logic_state_t input : inputs) {
			if (input == logic_state_t::LOW) {
				++numLow;
			} else if (input == logic_state_t::HIGH) {
				++numHigh;
			} else if (input == logic_state_t::FLOATING) {
				++numFloating;
			} else if (input == logic_state_t::UNDEFINED) {
				++numUndefined;
			}
		}
		int total = numLow + numHigh + numFloating + numUndefined;
		if (total == 0) {
			if (blockType == BlockType::JUNCTION) {
				return logic_state_t::FLOATING;
			}
			if (blockType == BlockType::JUNCTION_H) {
				return logic_state_t::HIGH;
			}
			if (blockType == BlockType::JUNCTION_X || blockType == BlockType::BUFFER || blockType == BlockType::NOT) {
				return logic_state_t::UNDEFINED;
			}
			return logic_state_t::LOW;
		}
		if (blockType == BlockType::AND) {
			if (numLow != 0) {
				return logic_state_t::LOW;
			}
			if (numUndefined != 0) {
				return logic_state_t::UNDEFINED;
			}
			if (numFloating != 0) {
				return logic_state_t::UNDEFINED;
			}
			return logic_state_t::HIGH;
		} else if (blockType == BlockType::OR) {
			if (numHigh != 0) {
				return logic_state_t::HIGH;
			}
			if (numUndefined != 0) {
				return logic_state_t::UNDEFINED;
			}
			if (numFloating != 0) {
				return logic_state_t::UNDEFINED;
			}
			return logic_state_t::LOW;
		} else if (blockType == BlockType::NAND) {
			if (numLow != 0) {
				return logic_state_t::HIGH;
			}
			if (numUndefined != 0) {
				return logic_state_t::UNDEFINED;
			}
			if (numFloating != 0) {
				return logic_state_t::UNDEFINED;
			}
			return logic_state_t::LOW;
		} else if (blockType == BlockType::NOR) {
			if (numHigh != 0) {
				return logic_state_t::LOW;
			}
			if (numUndefined != 0) {
				return logic_state_t::UNDEFINED;
			}
			if (numFloating != 0) {
				return logic_state_t::UNDEFINED;
			}
			return logic_state_t::HIGH;
		} else if (blockType == BlockType::XOR) {
			if (numUndefined != 0) {
				return logic_state_t::UNDEFINED;
			}
			if (numFloating != 0) {
				return logic_state_t::UNDEFINED;
			}
			bool parity = (numHigh % 2 == 1);
			if (parity) {
				return logic_state_t::HIGH;
			}
			return logic_state_t::LOW;
		} else if (blockType == BlockType::XNOR) {
			if (numUndefined != 0) {
				return logic_state_t::UNDEFINED;
			}
			if (numFloating != 0) {
				return logic_state_t::UNDEFINED;
			}
			bool parity = (numHigh % 2 == 0);
			if (parity) {
				return logic_state_t::HIGH;
			}
			return logic_state_t::LOW;
		} else if (blockType == BlockType::JUNCTION ||
				   blockType == BlockType::JUNCTION_L ||
				   blockType == BlockType::JUNCTION_H ||
				   blockType == BlockType::JUNCTION_X ||
				   blockType == BlockType::BUFFER ||
				   blockType == BlockType::NOT) {
			if (numUndefined != 0) {
				return logic_state_t::UNDEFINED;
			}
			bool hasHigh = numHigh > 0;
			bool hasLow = numLow > 0;
			if (hasHigh && hasLow) {
				return logic_state_t::UNDEFINED;
			} else if (hasHigh) {
				if (blockType == BlockType::NOT) {
					return logic_state_t::LOW;
				}
				return logic_state_t::HIGH;
			} else if (hasLow) {
				if (blockType == BlockType::NOT) {
					return logic_state_t::HIGH;
				}
				return logic_state_t::LOW;
			}
			if (blockType == BlockType::JUNCTION_L) {
				return logic_state_t::LOW;
			} else if (blockType == BlockType::JUNCTION_H) {
				return logic_state_t::HIGH;
			} else if (blockType == BlockType::JUNCTION_X || blockType == BlockType::BUFFER || blockType == BlockType::NOT) {
				return logic_state_t::UNDEFINED;
			}
			return logic_state_t::FLOATING;
		}
		return logic_state_t::UNDEFINED;
	};

	logic_state_t tristateBufferImplementation(std::vector<logic_state_t> enableInputs, std::vector<logic_state_t> dataInputs) {
		logic_state_t enableState = naiveButCorrectGateImplementation(BlockType::JUNCTION, enableInputs);
		if (enableState == logic_state_t::LOW) {
			return logic_state_t::FLOATING;
		} else if (enableState == logic_state_t::HIGH) {
			logic_state_t data = naiveButCorrectGateImplementation(BlockType::JUNCTION, dataInputs);
			if (data == logic_state_t::FLOATING) {
				return logic_state_t::UNDEFINED;
			} else {
				return data;
			}
		} else {
			return logic_state_t::UNDEFINED;
		}
	}
}

TEST_F(PrimitivesEvaluatorTest, AllBasicGatesBehavior) {
	struct Testcase {
		BlockType blockType;
		std::vector<logic_state_t> inputStates;
		Vector connectionOffset;
	};
	std::vector<Testcase> testcases = {};
	std::vector<logic_state_t> allStates = {
		logic_state_t::LOW,
		logic_state_t::HIGH,
		logic_state_t::FLOATING,
		logic_state_t::UNDEFINED
	};
	std::vector<BlockType> allTypes = {
		BlockType::AND,
		BlockType::OR,
		BlockType::XOR,
		BlockType::NAND,
		BlockType::NOR,
		BlockType::XNOR,
		BlockType::BUFFER,
		BlockType::NOT,
		BlockType::JUNCTION,
		BlockType::JUNCTION_L,
		BlockType::JUNCTION_H,
		BlockType::JUNCTION_X
	};
	std::vector<Vector> connectionOffset = {
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 2 },
		{ 0, 2 },
		{ 0, 2 }
	};
	for (unsigned int i = 0; i < allTypes.size(); ++i) {
		BlockType blockType = allTypes[i];
		testcases.push_back({ blockType, {}, connectionOffset[i] });
		for (logic_state_t state1 : allStates) {
			testcases.push_back({ blockType, {state1}, connectionOffset[i] });
			for (logic_state_t state2 : allStates) {
				testcases.push_back({ blockType, {state1, state2}, connectionOffset[i] });
				for (logic_state_t state3 : allStates) {
					testcases.push_back({ blockType, {state1, state2, state3}, connectionOffset[i] });
				}
			}
		}
	}
	BlockType currentType = BlockType::AND;
	int maxNumInputs = 0;
	for (Testcase testcase : testcases) {
		maxNumInputs = std::max(maxNumInputs, (int)(testcase.inputStates.size()));
	}
	for (int i = 0; i < maxNumInputs; ++i) {
		ASSERT_TRUE(circuit->tryInsertBlock(Position(0, i), Rotation::ZERO, BlockType::SWITCH));
	}
	ASSERT_TRUE(circuit->tryInsertBlock(Position(1, 0), Rotation::ZERO, BlockType::AND));
	for (Testcase testcase : testcases) {
		if (currentType != testcase.blockType) {
			ASSERT_TRUE(circuit->tryRemoveBlock(Position(1, 0)));
			ASSERT_TRUE(circuit->tryInsertBlock(Position(1, 0), Rotation::ZERO, testcase.blockType));
			currentType = testcase.blockType;
		}
		for (int i = 0; i < testcase.inputStates.size(); ++i) {
			ASSERT_TRUE(circuit->tryCreateConnection(Position(0, i), Position(1, 0) + testcase.connectionOffset));
			simulator->setState(Address({ 0, i }), testcase.inputStates[i]);
		}
		simulator->tickStep();
		logic_state_t expectedState = naiveButCorrectGateImplementation(testcase.blockType, testcase.inputStates);
		logic_state_t computedState = simulator->getState(Address(Position(1, 0)));
		EXPECT_EQ(expectedState, computedState) << " for block type " << blocktype_to_string(testcase.blockType) << " with inputs " << to_string(testcase.inputStates);
		for (int i = 0; i < testcase.inputStates.size(); ++i) {
			ASSERT_TRUE(circuit->tryRemoveConnection(Position(0, i), Position(1, 0) + testcase.connectionOffset));
		}
	}
}

TEST_F(PrimitivesEvaluatorTest, TristateBufferBehavior) {
	struct Testcase {
		std::vector<logic_state_t> enableStates;
		std::vector<logic_state_t> dataStates;
	};
	std::vector<Testcase> testcases = {};
	std::vector<logic_state_t> allStates = {
		logic_state_t::LOW,
		logic_state_t::HIGH,
		logic_state_t::FLOATING,
		logic_state_t::UNDEFINED
	};
	for (logic_state_t enableState : allStates) {
		for (logic_state_t dataState : allStates) {
			testcases.push_back({ { enableState }, { dataState } });
			for (logic_state_t dataState2 : allStates) {
				testcases.push_back({ { enableState }, { dataState, dataState2 } });
			}
			for (logic_state_t enableState2 : allStates) {
				testcases.push_back({ { enableState, enableState2 }, { dataState } });
			}
		}
	}
	for (int orientation = 0; orientation < 8; ++orientation) {
		for (Testcase testcase : testcases) {
			circuit->clear();
			ASSERT_TRUE(circuit->tryInsertBlock(Position(0, 0), Orientation(orientation), BlockType::TRISTATE_BUFFER));
			const Block* tristateBuffer = circuit->getBlockContainer().getBlock(Position(0, 0));
			ASSERT_NE(tristateBuffer, nullptr);
			int switchPos = 10;
			for (int i = 0; i < testcase.enableStates.size(); ++i) {
				ASSERT_TRUE(circuit->tryInsertBlock(Position(0, switchPos), Rotation::ZERO, BlockType::SWITCH));
				simulator->setState(Address(Position(0, switchPos)), testcase.enableStates[i]);
				std::optional<Position> connectionPos = tristateBuffer->getConnectionPosition(connection_end_id_t(1));
				ASSERT_TRUE(connectionPos.has_value());
				ASSERT_TRUE(circuit->tryCreateConnection(Position(0, switchPos), connectionPos.value()));
				++switchPos;
			}
			for (int i = 0; i < testcase.dataStates.size(); ++i) {
				ASSERT_TRUE(circuit->tryInsertBlock(Position(0, switchPos), Rotation::ZERO, BlockType::SWITCH));
				simulator->setState(Address(Position(0, switchPos)), testcase.dataStates[i]);
				std::optional<Position> connectionPos = tristateBuffer->getConnectionPosition(connection_end_id_t(0));
				ASSERT_TRUE(connectionPos.has_value());
				ASSERT_TRUE(circuit->tryCreateConnection(Position(0, switchPos), connectionPos.value()));
				++switchPos;
			}
			ASSERT_TRUE(circuit->tryInsertBlock(Position(5, 0), Rotation::ZERO, BlockType::LIGHT));
			std::optional<Position> connectionPos = tristateBuffer->getConnectionPosition(connection_end_id_t(2));
			ASSERT_TRUE(connectionPos.has_value());
			ASSERT_TRUE(circuit->tryCreateConnection(connectionPos.value(), Position(5, 0)));
			logic_state_t expectedState = tristateBufferImplementation(testcase.enableStates, testcase.dataStates);
			simulator->tickStep();
			logic_state_t computedState1 = simulator->getState(Position(5, 0));
			logic_state_t computedState2 = simulator->getState(Position(0, 0));
			EXPECT_EQ(expectedState, computedState1);
			EXPECT_EQ(expectedState, computedState2);
		}
	}
}
