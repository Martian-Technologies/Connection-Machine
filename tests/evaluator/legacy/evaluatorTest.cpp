#include <gtest/gtest.h>

#include "backend/address.h"
#include "environment/environment.h"
#include "backend/evaluator/evaluator.h"

class EvaluatorTest : public ::testing::Test {
public:
	void changeState(const Address& addr);
	void readState(const Address& addr) const;

protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment {false};
	SharedEvaluator evaluator = nullptr;
	SharedCircuit circuit = nullptr;
	int i;
	logic_state_t L = logic_state_t::LOW;
	logic_state_t H = logic_state_t::HIGH;
	logic_state_t Z = logic_state_t::FLOATING;
	logic_state_t X = logic_state_t::UNDEFINED;
};

// Note that logic simulator is tested separately
void EvaluatorTest::SetUp() {
	circuit_id_t circuitId = environment.getBackend().createCircuit();
	circuit = environment.getBackend().getCircuit(circuitId);
	auto id = environment.getBackend().createEvaluator(circuitId);
	evaluator = environment.getBackend().getEvaluator(id.value());
	i = 0;
}

void EvaluatorTest::TearDown() {
	// remove ptr references
	circuit.reset();
	evaluator.reset();
}

TEST_F(EvaluatorTest, InitTest) {
	// 0 when paused, set to be paused on evaluator's constructor
	ASSERT_EQ(evaluator->getRealTickrate(), 0);
}

TEST_F(EvaluatorTest, PauseUnpauseTest) {
	evaluator->setPause(false);
	evaluator->setUseTickrate(false);
	// set to 1000000000 tick/min
	// tickrate should be ~16666666.7 ?
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	ASSERT_GT(evaluator->getRealTickrate(), 0);

	evaluator->setPause(true);
	ASSERT_EQ(evaluator->getRealTickrate(), 0);
}

TEST_F(EvaluatorTest, TickrateTest) {
	double new_tickrate = 10.0;
	evaluator->setTickrate(new_tickrate);
	evaluator->setUseTickrate(true);
	evaluator->setPause(false);

	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	ASSERT_GT(evaluator->getRealTickrate(), 0);
	ASSERT_LT(evaluator->getRealTickrate(), new_tickrate*2);

	evaluator->setUseTickrate(false);
	evaluator->setPause(true);
}

TEST_F(EvaluatorTest, BasicStateManagement) {
	Position pos(i, i); ++i;
	Rotation rot = Rotation::ZERO;

	// insert switch
	circuit->tryInsertBlock(pos, rot, BlockType::SWITCH);

	Address addr(pos);
	ASSERT_EQ(evaluator->getState(addr), L);

	// set state
	evaluator->setState(addr, H);
	ASSERT_EQ(evaluator->getState(addr), H);
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
// 	std::vector<logic_state_t> states = evaluator->getBulkStates(addresses);
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
	evaluator->setState(Address(in1), H);
	evaluator->setState(Address(in2), H);

	// run simulation
	evaluator->tickStep();

	// check AND gate output
	ASSERT_EQ(evaluator->getState(Address(andPos)), H);

	// change one input
	evaluator->setState(Address(in1), L);
	evaluator->tickStep();
	ASSERT_EQ(evaluator->getState(Address(andPos)), L);

	evaluator->setPause(true);
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
	//ASSERT_EQ(evaluator->getState(removedAddr), false);

	// block movement
	evaluator->setState(pos2, H);
	Position newPos(i, i); ++i;
	circuit->tryMoveBlock(pos2, newPos, Orientation());

	// reaccess addresses
	//ASSERT_EQ(evaluator->getState(Address(pos2)), false); // This old address will cause an error as it is not found in address tree
	ASSERT_EQ(evaluator->getState(Address(newPos)), H);
}

TEST_F(EvaluatorTest, ThreadSafetyAndPausing) {
	Position pos(i, i); ++i;
	circuit->tryInsertBlock(pos, Rotation::ZERO, BlockType::SWITCH);
	Address addr(pos);

	// test rapid state changes
	evaluator->setPause(false);

	std::thread stateChanger([this, addr]() { changeState(addr); });
	std::thread stateReader([this, addr]() { readState(addr); });

	stateChanger.join();
	stateReader.join();

	logic_state_t finalState = evaluator->getState(addr);
	// the change state is ran 100 times, ending on 99%2==0
	ASSERT_TRUE(finalState == L) << "Not synced threads";

	evaluator->setPause(true);
}

void EvaluatorTest::changeState(const Address& addr) {
	for (int j = 0; j < 100; j++) {
		evaluator->setState(addr, fromBool(j % 2 == 0));
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
}

void EvaluatorTest::readState(const Address& addr) const {
	for (int j = 0; j < 100; ++j) {
		evaluator->getState(addr);
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
		ASSERT_NO_THROW(evaluator->getState(addr));
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
		evaluator->setState(Address({ 0, 0 }), fromBool((i & 1) == 1));
		evaluator->setState(Address({ 1, 0 }), fromBool((i & 2) == 2));
		evaluator->setState(Address({ 2, 0 }), fromBool((i & 4) == 4));
		evaluator->setState(Address({ 3, 0 }), fromBool((i & 8) == 8));
		for (int j = 0; j < 4; ++j) {
			evaluator->setState(Address({ 0, 1 }), fromBool((j & 1) == 1));
			evaluator->setState(Address({ 1, 1 }), fromBool((j & 2) == 2));
			evaluator->setState(Address({ 2, 1 }), fromBool((j & 4) == 4));
			evaluator->setState(Address({ 3, 1 }), fromBool((j & 8) == 8));
			evaluator->tickStep(2);
			ASSERT_EQ(evaluator->getState(Address({ 0, 2 })), fromBool((i & 1) == (j & 1)));
			ASSERT_EQ(evaluator->getState(Address({ 1, 2 })), fromBool((i & 2) == (j & 2)));
			ASSERT_EQ(evaluator->getState(Address({ 2, 2 })), fromBool((i & 4) == (j & 4)));
			ASSERT_EQ(evaluator->getState(Address({ 3, 2 })), fromBool((i & 8) == (j & 8)));
			ASSERT_EQ(evaluator->getState(Address({ 0, 3 })), fromBool(i == j));
		}
	}
}

TEST_F(EvaluatorTest, JunctionRemovalGate) {
	circuit->tryInsertBlock({ 0, 0 }, Rotation::ZERO, BlockType::SWITCH); // switch 1
	circuit->tryInsertBlock({ 1, 0 }, Rotation::ZERO, BlockType::SWITCH); // switch 2
	circuit->tryInsertBlock({ 2, 0 }, Rotation::ZERO, BlockType::JUNCTION); // junction 1
	circuit->tryInsertBlock({ 3, 0 }, Rotation::ZERO, BlockType::JUNCTION); // junction 2
	circuit->tryInsertBlock({ 4, 0 }, Rotation::ZERO, BlockType::AND);
	circuit->tryCreateConnection(Position { 0, 0 }, Position { 2, 0 }); // switch 1 to junction 1
	circuit->tryCreateConnection(Position { 1, 0 }, Position { 2, 0 }); // switch 2 to junction 1
	circuit->tryCreateConnection(Position { 3, 0 }, Position { 4, 0 }); // junction 2 to and gate
	circuit->tryCreateConnection(Position { 2, 0 }, Position { 3, 0 }); // junction 1 to junction 2
	ASSERT_EQ(evaluator->getState(Address({ 0, 0 })), L);
	ASSERT_EQ(evaluator->getState(Address({ 1, 0 })), L);
	ASSERT_EQ(evaluator->getState(Address({ 2, 0 })), L);
	ASSERT_EQ(evaluator->getState(Address({ 3, 0 })), L);
	ASSERT_EQ(evaluator->getState(Address({ 4, 0 })), L);
	evaluator->setState(Address({ 0, 0 }), H);
	evaluator->tickStep(1);
	ASSERT_EQ(evaluator->getState(Address({ 0, 0 })), H);
	ASSERT_EQ(evaluator->getState(Address({ 1, 0 })), L);
	ASSERT_EQ(evaluator->getState(Address({ 2, 0 })), X);
	ASSERT_EQ(evaluator->getState(Address({ 3, 0 })), X);
	ASSERT_EQ(evaluator->getState(Address({ 4, 0 })), X);
	evaluator->setState(Address({ 1, 0 }), H);
	evaluator->tickStep(1);
	ASSERT_EQ(evaluator->getState(Address({ 0, 0 })), H);
	ASSERT_EQ(evaluator->getState(Address({ 1, 0 })), H);
	ASSERT_EQ(evaluator->getState(Address({ 2, 0 })), H);
	ASSERT_EQ(evaluator->getState(Address({ 3, 0 })), H);
	ASSERT_EQ(evaluator->getState(Address({ 4, 0 })), H);
	evaluator->setState(Address({ 0, 0 }), L);
	evaluator->tickStep(1);
	ASSERT_EQ(evaluator->getState(Address({ 0, 0 })), L);
	ASSERT_EQ(evaluator->getState(Address({ 1, 0 })), H);
	ASSERT_EQ(evaluator->getState(Address({ 2, 0 })), X);
	ASSERT_EQ(evaluator->getState(Address({ 3, 0 })), X);
	ASSERT_EQ(evaluator->getState(Address({ 4, 0 })), X);
	circuit->tryRemoveBlock(Position { 0, 0 });
	ASSERT_EQ(evaluator->getState(Address({ 1, 0 })), H);
	ASSERT_EQ(evaluator->getState(Address({ 2, 0 })), H);
	ASSERT_EQ(evaluator->getState(Address({ 3, 0 })), H);
	ASSERT_EQ(evaluator->getState(Address({ 4, 0 })), X);
	evaluator->tickStep(1);
	ASSERT_EQ(evaluator->getState(Address({ 1, 0 })), H);
	ASSERT_EQ(evaluator->getState(Address({ 2, 0 })), H);
	ASSERT_EQ(evaluator->getState(Address({ 3, 0 })), H);
	ASSERT_EQ(evaluator->getState(Address({ 4, 0 })), H);
	circuit->tryRemoveConnection(Position { 2, 0 }, Position { 3, 0 });
	ASSERT_EQ(evaluator->getState(Address({ 1, 0 })), H);
	ASSERT_EQ(evaluator->getState(Address({ 2, 0 })), H);
	ASSERT_EQ(evaluator->getState(Address({ 3, 0 })), Z);
	ASSERT_EQ(evaluator->getState(Address({ 4, 0 })), H);
	evaluator->tickStep(1);
	ASSERT_EQ(evaluator->getState(Address({ 1, 0 })), H);
	ASSERT_EQ(evaluator->getState(Address({ 2, 0 })), H);
	ASSERT_EQ(evaluator->getState(Address({ 3, 0 })), Z);
	ASSERT_EQ(evaluator->getState(Address({ 4, 0 })), X);
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

	evaluator->setState(Address(dataPos), L);
	evaluator->setState(Address(enablePos), L);
	evaluator->tickStep(2);
	EXPECT_EQ(evaluator->getState(Address(tristatePos)), Z);
	EXPECT_EQ(evaluator->getState(Address(lightPos)), Z);

	evaluator->setState(Address(dataPos), H);
	evaluator->tickStep(2);
	EXPECT_EQ(evaluator->getState(Address(tristatePos)), Z);
	EXPECT_EQ(evaluator->getState(Address(lightPos)), Z);

	evaluator->setState(Address(enablePos), H);
	evaluator->tickStep(2);
	EXPECT_EQ(evaluator->getState(Address(tristatePos)), H);
	EXPECT_EQ(evaluator->getState(Address(lightPos)), H);

	evaluator->setState(Address(dataPos), L);
	evaluator->tickStep(2);
	EXPECT_EQ(evaluator->getState(Address(tristatePos)), L);
	EXPECT_EQ(evaluator->getState(Address(lightPos)), L);

	evaluator->setState(Address(enablePos), L);
	evaluator->tickStep(2);
	EXPECT_EQ(evaluator->getState(Address(tristatePos)), Z);
	EXPECT_EQ(evaluator->getState(Address(lightPos)), Z);
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
	evaluator->setState(Address(dataPosA), L);
	evaluator->setState(Address(enablePosA), L);
	evaluator->setState(Address(dataPosB), L);
	evaluator->setState(Address(enablePosB), L);
	evaluator->tickStep(2);
	EXPECT_EQ(evaluator->getState(Address(tristatePosA)), Z);
	EXPECT_EQ(evaluator->getState(Address(tristatePosB)), Z);
	EXPECT_EQ(evaluator->getState(Address(junctionPos)), Z);
	EXPECT_EQ(evaluator->getState(Address(lightPos)), Z);

	// enable A: junction follows A's data
	evaluator->setState(Address(dataPosA), H);
	evaluator->setState(Address(enablePosA), H);
	evaluator->tickStep(2);
	EXPECT_EQ(evaluator->getState(Address(tristatePosA)), H);
	EXPECT_EQ(evaluator->getState(Address(junctionPos)), H);
	EXPECT_EQ(evaluator->getState(Address(lightPos)), H);

	// disable A, bus should float again
	evaluator->setState(Address(enablePosA), L);
	evaluator->tickStep(2);
	EXPECT_EQ(evaluator->getState(Address(tristatePosA)), Z);
	EXPECT_EQ(evaluator->getState(Address(junctionPos)), Z);
	EXPECT_EQ(evaluator->getState(Address(lightPos)), Z);

	// enable B: junction follows B
	evaluator->setState(Address(dataPosB), H);
	evaluator->setState(Address(enablePosB), H);
	evaluator->tickStep(2);
	EXPECT_EQ(evaluator->getState(Address(tristatePosB)), H);
	EXPECT_EQ(evaluator->getState(Address(junctionPos)), H);
	EXPECT_EQ(evaluator->getState(Address(lightPos)), H);

	// contention: A drives LOW, B drives HIGH
	evaluator->setState(Address(dataPosA), L);
	evaluator->setState(Address(enablePosA), H);
	evaluator->tickStep(2);
	EXPECT_EQ(evaluator->getState(Address(tristatePosA)), L);
	EXPECT_EQ(evaluator->getState(Address(tristatePosB)), H);
	EXPECT_EQ(evaluator->getState(Address(junctionPos)), X);
	EXPECT_EQ(evaluator->getState(Address(lightPos)), X);

	// remove tri-state A while replacement mappings exist
	ASSERT_TRUE(circuit->tryRemoveBlock(tristatePosA));
	evaluator->tickStep(2);
	EXPECT_EQ(evaluator->getState(Address(junctionPos)), H);
	EXPECT_EQ(evaluator->getState(Address(lightPos)), H);

	// disable remaining driver -> bus floats
	evaluator->setState(Address(enablePosB), L);
	evaluator->tickStep(2);
	EXPECT_EQ(evaluator->getState(Address(junctionPos)), Z);
	EXPECT_EQ(evaluator->getState(Address(lightPos)), Z);
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
		evaluator->setState(Position(i, 0), H);
	}
	int j = 0;
	for (BlockType type : allTypes) {
		++j;
		for (i = 0; i < LARGE_NUMBER; i++) {
			circuit->tryInsertBlock(Position(i, j), Rotation::ZERO, type);
			circuit->tryCreateConnection(Position(i, 0), Position(i, j));
		}
	}
	evaluator->tickStep(1);
	Position andGate(rand() % LARGE_NUMBER, 1);
	// and on should be high
	ASSERT_EQ(evaluator->getState(Address( {rand() % LARGE_NUMBER, 1})), H);
	// or on should be high
	ASSERT_EQ(evaluator->getState(Address( {rand() % LARGE_NUMBER, 2})), H);
	// xor on should be high
	ASSERT_EQ(evaluator->getState(Address( {rand() % LARGE_NUMBER, 3})), H);
	// nand on should be low
	ASSERT_EQ(evaluator->getState(Address( {rand() % LARGE_NUMBER, 4})), L);
	// nor on should be low
	ASSERT_EQ(evaluator->getState(Address( {rand() % LARGE_NUMBER, 5})), L);
	// xnor on should be low
	ASSERT_EQ(evaluator->getState(Address( {rand() % LARGE_NUMBER, 6})), L);
}
