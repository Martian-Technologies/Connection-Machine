#include <gtest/gtest.h>

#include "backend/address.h"
#include "environment/environment.h"
#include "backend/evaluator/simulator/evalLogicSimulator.h"

class EvaluatorTest : public ::testing::Test {
public:
	void changeState(const Address& addr);
	void readState(const Address& addr) const;

protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment {false};
	EvalLogicSimulator* simulator = nullptr;
	Circuit* circuit = nullptr;
	int i;
	logic_state_t L = logic_state_t::LOW;
	logic_state_t H = logic_state_t::HIGH;
	logic_state_t Z = logic_state_t::FLOATING;
	logic_state_t X = logic_state_t::UNDEFINED;
};

// Note that logic simulator is tested separately
void EvaluatorTest::SetUp() {
	circuit_id_t circuitId = environment.getBackend().createCircuit();
	circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
	auto id = environment.getBackend().createSimulator(circuitId);
	simulator = environment.getBackend().getSimulator(id.value());
	i = 0;
}

void EvaluatorTest::TearDown() {
	// remove ptr references
	circuit = nullptr;
	simulator = nullptr;
}

TEST_F(EvaluatorTest, InitTest) {
	// 0 when paused, set to be paused on simulator's constructor
	ASSERT_EQ(simulator->getRealTickrate(), 0);
}

TEST_F(EvaluatorTest, PauseUnpauseTest) {
	simulator->setPause(false);
	simulator->setUseTickrate(false);
	// set to 1000000000 tick/min
	// tickrate should be ~16666666.7 ?
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	ASSERT_GT(simulator->getRealTickrate(), 0);

	simulator->setPause(true);
	ASSERT_EQ(simulator->getRealTickrate(), 0);
}

TEST_F(EvaluatorTest, TickrateTest) {
	double new_tickrate = 10.0;
	simulator->setTickrate(new_tickrate);
	simulator->setUseTickrate(true);
	simulator->setPause(false);

	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	ASSERT_GT(simulator->getRealTickrate(), 0);
	ASSERT_LT(simulator->getRealTickrate(), new_tickrate*2);

	simulator->setUseTickrate(false);
	simulator->setPause(true);
}

TEST_F(EvaluatorTest, BasicStateManagement) {
	Position pos(i, i); ++i;
	Rotation rot = Rotation::ZERO;

	// insert switch
	circuit->tryInsertBlock(pos, rot, BlockType::SWITCH);

	Address addr(pos);
	ASSERT_EQ(simulator->getState(addr), L);

	// set state
	simulator->setState(addr, H);
	ASSERT_EQ(simulator->getState(addr), H);
}

// TEST_F(EvaluatorTest, BulkStateOperations) {
// 	std::vector<Position> positions = {
// 		Position(i, i),
// 		Position(i + 1, i),
// 		Position(i + 2, i)
// 	};
// 	i += 3;
//
// 	// place switches
// 	for (const Position& pos : positions) {
// 		circuit->tryInsertBlock(pos, Rotation::ZERO, BlockType::SWITCH);
// 	}
//
// 	std::vector<Address> addresses;
// 	for (const Position& pos : positions) {
// 		addresses.push_back(Address(pos));
// 	}
//
// 	std::vector<logic_state_t> states = simulator->getBulkStates(addresses);
// 	ASSERT_EQ(states.size(), addresses.size());
// 	for (logic_state_t state : states) {
// 		ASSERT_EQ(state, L);
// 	}
// }

TEST_F(EvaluatorTest, LogicGateEvaluation) {
	// AND gate with two inputs
	Position andPos(i, i); ++i;
	Position in1(i, i); ++i;
	Position in2(i, i); ++i;

	circuit->tryInsertBlock(andPos, Rotation::ZERO, BlockType::AND);
	circuit->tryInsertBlock(in1, Rotation::ZERO, BlockType::SWITCH);
	circuit->tryInsertBlock(in2, Rotation::ZERO, BlockType::SWITCH);

	circuit->tryCreateConnection(in1, andPos);
	circuit->tryCreateConnection(in2, andPos);

	// set input states
	simulator->setState(Address(in1), H);
	simulator->setState(Address(in2), H);

	// run simulation
	simulator->tickStep();

	// check AND gate output
	ASSERT_EQ(simulator->getState(Address(andPos)), H);

	// change one input
	simulator->setState(Address(in1), L);
	simulator->tickStep();
	ASSERT_EQ(simulator->getState(Address(andPos)), L);

	simulator->setPause(true);
}

TEST_F(EvaluatorTest, EvaluatingCircuitModifications) {
	Position pos1(i, i); ++i;
	Position pos2(i, i); ++i;

	circuit->tryInsertBlock(pos1, Rotation::ZERO, BlockType::AND);
	circuit->tryInsertBlock(pos2, Rotation::ZERO, BlockType::OR);
	circuit->tryCreateConnection(pos1, pos2);

	// removal
	circuit->tryRemoveBlock(pos1);

	// retrieving state from address should have a check if it doesn't exist
	// As of now we don't have a check and we attempt to retrieve a key that doesn't exist in unordered map
	//Address removedAddr(pos1);
	//ASSERT_EQ(simulator->getState(removedAddr), false);

	// block movement
	simulator->setState(pos2, H);
	Position newPos(i, i); ++i;
	circuit->tryMoveBlock(pos2, newPos, Orientation());

	// reaccess addresses
	//ASSERT_EQ(simulator->getState(Address(pos2)), false); // This old address will cause an error as it is not found in address tree
	ASSERT_EQ(simulator->getState(Address(newPos)), H);
}

TEST_F(EvaluatorTest, DISABLED_ThreadSafetyAndPausing) {
	Position pos(i, i); ++i;
	circuit->tryInsertBlock(pos, Rotation::ZERO, BlockType::SWITCH);
	Address addr(pos);

	// test rapid state changes
	simulator->setPause(false);

	std::thread stateChanger([this, addr]() { changeState(addr); });
	std::thread stateReader([this, addr]() { readState(addr); });

	stateChanger.join();
	stateReader.join();

	logic_state_t finalState = simulator->getState(addr);
	// the change state is ran 100 times, ending on 99%2==0
	ASSERT_TRUE(finalState == L) << "Not synced threads";

	simulator->setPause(true);
}

void EvaluatorTest::changeState(const Address& addr) {
	for (int j = 0; j < 100; j++) {
		simulator->setState(addr, fromBool(j % 2 == 0));
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
}

void EvaluatorTest::readState(const Address& addr) const {
	for (int j = 0; j < 100; ++j) {
		simulator->getState(addr);
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
}

TEST_F(EvaluatorTest, FastCircuitModifications) {
	std::vector<Position> positions;
	for (int j = 0; j < 10; ++j) {
		positions.push_back(Position(i, i)); ++i;
		circuit->tryInsertBlock(positions.back(), Rotation::ZERO, BlockType::AND);
	}

	for (int j = 0; j < (int)positions.size() - 1; ++j) {
		circuit->tryCreateConnection(positions[j], positions[j + 1]);
	}

	for (int j = 0; j < (int)positions.size(); j += 2) {
		circuit->tryRemoveBlock(positions[j]);
	}

	// check that blocks still exist
	for (int j = 1; j < (int)positions.size(); j += 2) {
		Address addr(positions[j]);
		ASSERT_NO_THROW(simulator->getState(addr));
	}
}

TEST_F(EvaluatorTest, EqualityCircuit) {
	circuit->tryInsertBlock({ 0, 3 }, Rotation::ZERO, BlockType::AND); // output
	for (int i = 0; i < 4; ++i) {
		circuit->tryInsertBlock({i, 0}, Rotation::ZERO, BlockType::SWITCH);
		circuit->tryInsertBlock({i, 1}, Rotation::ZERO, BlockType::SWITCH);
		circuit->tryInsertBlock({ i, 2 }, Rotation::ZERO, BlockType::XNOR); // compare two inputs are equal
		circuit->tryCreateConnection(Position { i, 0 }, Position { i, 2 });
		circuit->tryCreateConnection(Position { i, 1 }, Position { i, 2 });
		circuit->tryCreateConnection(Position { i, 2 }, Position { 0, 3 });
	}
	for (int i = 0; i < 4; ++i) {
		simulator->setState(Address({ 0, 0 }), fromBool((i & 1) == 1));
		simulator->setState(Address({ 1, 0 }), fromBool((i & 2) == 2));
		simulator->setState(Address({ 2, 0 }), fromBool((i & 4) == 4));
		simulator->setState(Address({ 3, 0 }), fromBool((i & 8) == 8));
		for (int j = 0; j < 4; ++j) {
			simulator->setState(Address({ 0, 1 }), fromBool((j & 1) == 1));
			simulator->setState(Address({ 1, 1 }), fromBool((j & 2) == 2));
			simulator->setState(Address({ 2, 1 }), fromBool((j & 4) == 4));
			simulator->setState(Address({ 3, 1 }), fromBool((j & 8) == 8));
			simulator->tickStep(2);
			ASSERT_EQ(simulator->getState(Address({ 0, 2 })), fromBool((i & 1) == (j & 1)));
			ASSERT_EQ(simulator->getState(Address({ 1, 2 })), fromBool((i & 2) == (j & 2)));
			ASSERT_EQ(simulator->getState(Address({ 2, 2 })), fromBool((i & 4) == (j & 4)));
			ASSERT_EQ(simulator->getState(Address({ 3, 2 })), fromBool((i & 8) == (j & 8)));
			ASSERT_EQ(simulator->getState(Address({ 0, 3 })), fromBool(i == j));
		}
	}
}

TEST_F(EvaluatorTest, JunctionRemovalGate) {
	Position switch1(0, 0);
	Position switch2(1, 0);
	Position junction1(2, 0);
	Position junction2(3, 0);
	Position andGate(4, 0);
	circuit->tryInsertBlock(switch1, Rotation::ZERO, BlockType::SWITCH); // switch 1
	circuit->tryInsertBlock(switch2, Rotation::ZERO, BlockType::SWITCH); // switch 2
	circuit->tryInsertBlock(junction1, Rotation::ZERO, BlockType::JUNCTION); // junction 1
	circuit->tryInsertBlock(junction2, Rotation::ZERO, BlockType::JUNCTION); // junction 2
	circuit->tryInsertBlock(andGate, Rotation::ZERO, BlockType::AND);
	circuit->tryCreateConnection(switch1, junction1); // switch 1 to junction 1
	circuit->tryCreateConnection(switch2, junction1); // switch 2 to junction 1
	circuit->tryCreateConnection(junction2, andGate); // junction 2 to and gate
	circuit->tryCreateConnection(junction1, junction2); // junction 1 to junction 2
	ASSERT_EQ(simulator->getState(Address(switch1)), L);
	ASSERT_EQ(simulator->getState(Address(switch2)), L);
	ASSERT_EQ(simulator->getState(Address(junction1)), L);
	ASSERT_EQ(simulator->getState(Address(junction2)), L);
	ASSERT_EQ(simulator->getState(Address(andGate)), L);
	simulator->setState(Address(switch1), H);
	simulator->tickStep(1);
	ASSERT_EQ(simulator->getState(Address(switch1)), H);
	ASSERT_EQ(simulator->getState(Address(switch2)), L);
	ASSERT_EQ(simulator->getState(Address(junction1)), X);
	ASSERT_EQ(simulator->getState(Address(junction2)), X);
	ASSERT_EQ(simulator->getState(Address(andGate)), X);
	simulator->setState(Address(switch2), H);
	simulator->tickStep(1);
	ASSERT_EQ(simulator->getState(Address(switch1)), H);
	ASSERT_EQ(simulator->getState(Address(switch2)), H);
	ASSERT_EQ(simulator->getState(Address(junction1)), H);
	ASSERT_EQ(simulator->getState(Address(junction2)), H);
	ASSERT_EQ(simulator->getState(Address(andGate)), H);
	simulator->setState(Address(switch1), L);
	simulator->tickStep(1);
	ASSERT_EQ(simulator->getState(Address(switch1)), L);
	ASSERT_EQ(simulator->getState(Address(switch2)), H);
	ASSERT_EQ(simulator->getState(Address(junction1)), X);
	ASSERT_EQ(simulator->getState(Address(junction2)), X);
	ASSERT_EQ(simulator->getState(Address(andGate)), X);
	circuit->tryRemoveBlock(switch1);
	ASSERT_EQ(simulator->getState(Address(switch2)), H);
	ASSERT_EQ(simulator->getState(Address(junction1)), H);
	ASSERT_EQ(simulator->getState(Address(junction2)), H);
	ASSERT_EQ(simulator->getState(Address(andGate)), X);
	simulator->tickStep(1);
	ASSERT_EQ(simulator->getState(Address(switch2)), H);
	ASSERT_EQ(simulator->getState(Address(junction1)), H);
	ASSERT_EQ(simulator->getState(Address(junction2)), H);
	ASSERT_EQ(simulator->getState(Address(andGate)), H);
	circuit->tryRemoveConnection(junction1, junction2);
	ASSERT_EQ(simulator->getState(Address(switch2)), H);
	ASSERT_EQ(simulator->getState(Address(junction1)), H);
	ASSERT_EQ(simulator->getState(Address(junction2)), Z);
	ASSERT_EQ(simulator->getState(Address(andGate)), H);
	simulator->tickStep(1);
	ASSERT_EQ(simulator->getState(Address(switch2)), H);
	ASSERT_EQ(simulator->getState(Address(junction1)), H);
	ASSERT_EQ(simulator->getState(Address(junction2)), Z);
	ASSERT_EQ(simulator->getState(Address(andGate)), X);
}

TEST_F(EvaluatorTest, TristateBufferEnableControlsOutput) {
	Position dataPos { 0, 0 };
	Position enablePos { 0, 2 };
	Position tristatePos { 2, 0 };
	Position lightPos { 4, 0 };
	Position tristateDataPortPos { tristatePos.x, tristatePos.y + 1 };
	Position tristateEnablePortPos { tristatePos };
	Position tristateOutputPortPos { tristatePos.x, tristatePos.y + 1 };

	circuit->tryInsertBlock(dataPos, Rotation::ZERO, BlockType::SWITCH);
	circuit->tryInsertBlock(enablePos, Rotation::ZERO, BlockType::SWITCH);
	circuit->tryInsertBlock(tristatePos, Rotation::ZERO, BlockType::TRISTATE_BUFFER);
	circuit->tryInsertBlock(lightPos, Rotation::ZERO, BlockType::LIGHT);

	ASSERT_TRUE(circuit->tryCreateConnection(dataPos, tristateDataPortPos));
	ASSERT_TRUE(circuit->tryCreateConnection(enablePos, tristateEnablePortPos));
	ASSERT_TRUE(circuit->tryCreateConnection(tristateOutputPortPos, lightPos));

	simulator->setState(Address(dataPos), L);
	simulator->setState(Address(enablePos), L);
	simulator->tickStep(2);
	EXPECT_EQ(simulator->getState(Address(tristatePos)), Z);
	EXPECT_EQ(simulator->getState(Address(lightPos)), Z);

	simulator->setState(Address(dataPos), H);
	simulator->tickStep(2);
	EXPECT_EQ(simulator->getState(Address(tristatePos)), Z);
	EXPECT_EQ(simulator->getState(Address(lightPos)), Z);

	simulator->setState(Address(enablePos), H);
	simulator->tickStep(2);
	EXPECT_EQ(simulator->getState(Address(tristatePos)), H);
	EXPECT_EQ(simulator->getState(Address(lightPos)), H);

	simulator->setState(Address(dataPos), L);
	simulator->tickStep(2);
	EXPECT_EQ(simulator->getState(Address(tristatePos)), L);
	EXPECT_EQ(simulator->getState(Address(lightPos)), L);

	simulator->setState(Address(enablePos), L);
	simulator->tickStep(2);
	EXPECT_EQ(simulator->getState(Address(tristatePos)), Z);
	EXPECT_EQ(simulator->getState(Address(lightPos)), Z);
}

TEST_F(EvaluatorTest, TristateBuffersResolveContentionOnJunction) {
	Position dataPosA { 0, 0 };
	Position enablePosA { 0, 2 };
	Position tristatePosA { 2, 0 };
	Position dataPosB { 0, 4 };
	Position enablePosB { 0, 6 };
	Position tristatePosB { 2, 4 };
	Position junctionPos { 4, 2 };
	Position lightPos { 6, 2 };
	Position tristateDataPortPosA { tristatePosA.x, tristatePosA.y + 1 };
	Position tristateEnablePortPosA { tristatePosA };
	Position tristateOutputPortPosA { tristatePosA.x, tristatePosA.y + 1 };
	Position tristateDataPortPosB { tristatePosB.x, tristatePosB.y + 1 };
	Position tristateEnablePortPosB { tristatePosB };
	Position tristateOutputPortPosB { tristatePosB.x, tristatePosB.y + 1 };

	circuit->tryInsertBlock(dataPosA, Rotation::ZERO, BlockType::SWITCH);
	circuit->tryInsertBlock(enablePosA, Rotation::ZERO, BlockType::SWITCH);
	circuit->tryInsertBlock(tristatePosA, Rotation::ZERO, BlockType::TRISTATE_BUFFER);
	circuit->tryInsertBlock(dataPosB, Rotation::ZERO, BlockType::SWITCH);
	circuit->tryInsertBlock(enablePosB, Rotation::ZERO, BlockType::SWITCH);
	circuit->tryInsertBlock(tristatePosB, Rotation::ZERO, BlockType::TRISTATE_BUFFER);
	circuit->tryInsertBlock(junctionPos, Rotation::ZERO, BlockType::JUNCTION);
	circuit->tryInsertBlock(lightPos, Rotation::ZERO, BlockType::LIGHT);

	ASSERT_TRUE(circuit->tryCreateConnection(dataPosA, tristateDataPortPosA));
	ASSERT_TRUE(circuit->tryCreateConnection(enablePosA, tristateEnablePortPosA));
	ASSERT_TRUE(circuit->tryCreateConnection(dataPosB, tristateDataPortPosB));
	ASSERT_TRUE(circuit->tryCreateConnection(enablePosB, tristateEnablePortPosB));
	ASSERT_TRUE(circuit->tryCreateConnection(tristateOutputPortPosA, junctionPos));
	ASSERT_TRUE(circuit->tryCreateConnection(tristateOutputPortPosB, junctionPos));
	ASSERT_TRUE(circuit->tryCreateConnection(junctionPos, lightPos));

	// baseline: everything low / disabled
	simulator->setState(Address(dataPosA), L);
	simulator->setState(Address(enablePosA), L);
	simulator->setState(Address(dataPosB), L);
	simulator->setState(Address(enablePosB), L);
	simulator->tickStep(2);
	EXPECT_EQ(simulator->getState(Address(tristatePosA)), Z);
	EXPECT_EQ(simulator->getState(Address(tristatePosB)), Z);
	EXPECT_EQ(simulator->getState(Address(junctionPos)), Z);
	EXPECT_EQ(simulator->getState(Address(lightPos)), Z);

	// enable A: junction follows A's data
	simulator->setState(Address(dataPosA), H);
	simulator->setState(Address(enablePosA), H);
	simulator->tickStep(2);
	EXPECT_EQ(simulator->getState(Address(tristatePosA)), H);
	EXPECT_EQ(simulator->getState(Address(junctionPos)), H);
	EXPECT_EQ(simulator->getState(Address(lightPos)), H);

	// disable A, bus should float again
	simulator->setState(Address(enablePosA), L);
	simulator->tickStep(2);
	EXPECT_EQ(simulator->getState(Address(tristatePosA)), Z);
	EXPECT_EQ(simulator->getState(Address(junctionPos)), Z);
	EXPECT_EQ(simulator->getState(Address(lightPos)), Z);

	// enable B: junction follows B
	simulator->setState(Address(dataPosB), H);
	simulator->setState(Address(enablePosB), H);
	simulator->tickStep(2);
	EXPECT_EQ(simulator->getState(Address(tristatePosB)), H);
	EXPECT_EQ(simulator->getState(Address(junctionPos)), H);
	EXPECT_EQ(simulator->getState(Address(lightPos)), H);

	// contention: A drives LOW, B drives HIGH
	simulator->setState(Address(dataPosA), L);
	simulator->setState(Address(enablePosA), H);
	simulator->tickStep(2);
	EXPECT_EQ(simulator->getState(Address(tristatePosA)), L);
	EXPECT_EQ(simulator->getState(Address(tristatePosB)), H);
	EXPECT_EQ(simulator->getState(Address(junctionPos)), X);
	EXPECT_EQ(simulator->getState(Address(lightPos)), X);

	// remove tri-state A while replacement mappings exist
	ASSERT_TRUE(circuit->tryRemoveBlock(tristatePosA));
	simulator->tickStep(2);
	EXPECT_EQ(simulator->getState(Address(junctionPos)), H);
	EXPECT_EQ(simulator->getState(Address(lightPos)), H);

	// disable remaining driver -> bus floats
	simulator->setState(Address(enablePosB), L);
	simulator->tickStep(2);
	EXPECT_EQ(simulator->getState(Address(junctionPos)), Z);
	EXPECT_EQ(simulator->getState(Address(lightPos)), Z);
}

TEST_F(EvaluatorTest, LargeEvaluatorTest) {
	int LARGE_NUMBER = 10;
	// 10 is about 20 ms
	// 100 is 200 ms
	// 1000 = EvaluatorTest.LargeEvaluatorTest (10790 ms) this seems like a
	// long time for relatively not a lot of stuff but idk
	std::vector<BlockType> allTypes = {
		BlockType::AND,
		BlockType::OR,
		BlockType::XOR,
		BlockType::NAND,
		BlockType::NOR,
		BlockType::XNOR
	};

	for (i = 0; i < LARGE_NUMBER; i++) {
		circuit->tryInsertBlock(Position(i, 0), Rotation::ZERO, BlockType::SWITCH);
		simulator->setState(Position(i, 0), H);
	}
	int j = 0;
	for (BlockType type : allTypes) {
		++j;
		for (i = 0; i < LARGE_NUMBER; i++) {
			circuit->tryInsertBlock(Position(i, j), Rotation::ZERO, type);
			circuit->tryCreateConnection(Position(i, 0), Position(i, j));
		}
	}
	simulator->tickStep(1);
	Position andGate(rand() % LARGE_NUMBER, 1);
	// and on should be high
	ASSERT_EQ(simulator->getState(Address( {rand() % LARGE_NUMBER, 1})), H);
	// or on should be high
	ASSERT_EQ(simulator->getState(Address( {rand() % LARGE_NUMBER, 2})), H);
	// xor on should be high
	ASSERT_EQ(simulator->getState(Address( {rand() % LARGE_NUMBER, 3})), H);
	// nand on should be low
	ASSERT_EQ(simulator->getState(Address( {rand() % LARGE_NUMBER, 4})), L);
	// nor on should be low
	ASSERT_EQ(simulator->getState(Address( {rand() % LARGE_NUMBER, 5})), L);
	// xnor on should be low
	ASSERT_EQ(simulator->getState(Address( {rand() % LARGE_NUMBER, 6})), L);
}
