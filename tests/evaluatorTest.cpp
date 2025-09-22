#include "evaluatorTest.h"

// Note that logic simulator is tested separately
void EvaluatorTest::SetUp() {
	circuit_id_t circuitId = backend.createCircuit();
	circuit = backend.getCircuit(circuitId);
	auto id = backend.createEvaluator(circuitId);
	evaluator = backend.getEvaluator(id.value());
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
	ASSERT_EQ(evaluator->getState(addr), logic_state_t::LOW);

	// set state
	evaluator->setState(addr, logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(addr), logic_state_t::HIGH);
}

// TEST_F(EvaluatorTest, BulkStateOperations) {
// 	std::vector<Position> positions = {
// 		Position(i, i),
// 		Position(i + 1, i),
// 		Position(i + 2, i)
// 	};
// 	i += 3;

// 	// place switches
// 	for (const Position& pos : positions) {
// 		circuit->tryInsertBlock(pos, Rotation::ZERO, BlockType::SWITCH);
// 	}

// 	std::vector<Address> addresses;
// 	for (const Position& pos : positions) {
// 		addresses.push_back(Address(pos));
// 	}

// 	std::vector<logic_state_t> states = evaluator->getBulkStates(addresses);
// 	ASSERT_EQ(states.size(), addresses.size());
// 	for (logic_state_t state : states) {
// 		ASSERT_EQ(state, logic_state_t::LOW);
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
	evaluator->setState(Address(in1), logic_state_t::HIGH);
	evaluator->setState(Address(in2), logic_state_t::HIGH);

	// run simulation
	evaluator->tickStep();

	// check AND gate output
	ASSERT_EQ(evaluator->getState(Address(andPos)), logic_state_t::HIGH);

	// change one input
	evaluator->setState(Address(in1), logic_state_t::LOW);
	evaluator->tickStep();
	ASSERT_EQ(evaluator->getState(Address(andPos)), logic_state_t::LOW);

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
	evaluator->setState(pos2, true);
	Position newPos(i, i); ++i;
	circuit->tryMoveBlock(pos2, newPos);

	// reaccess addresses
	//ASSERT_EQ(evaluator->getState(Address(pos2)), false); // This old address will cause an error as it is not found in address tree
	ASSERT_EQ(evaluator->getState(Address(newPos)), logic_state_t::HIGH);
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
	ASSERT_TRUE(finalState == logic_state_t::LOW) << "Not synced threads";

	evaluator->setPause(true);
}

void EvaluatorTest::changeState(const Address& addr) {
	for (int j = 0; j < 100; j++) {
		evaluator->setState(addr, j % 2 == 0);
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
		evaluator->setState(Address({ 0, 0 }), (i & 1) == 1);
		evaluator->setState(Address({ 1, 0 }), (i & 2) == 2);
		evaluator->setState(Address({ 2, 0 }), (i & 4) == 4);
		evaluator->setState(Address({ 3, 0 }), (i & 8) == 8);
		for (int j = 0; j < 4; ++j) {
			evaluator->setState(Address({ 0, 1 }), (j & 1) == 1);
			evaluator->setState(Address({ 1, 1 }), (j & 2) == 2);
			evaluator->setState(Address({ 2, 1 }), (j & 4) == 4);
			evaluator->setState(Address({ 3, 1 }), (j & 8) == 8);
			evaluator->tickStep(2);
			ASSERT_EQ(evaluator->getBoolState(Address({ 0, 2 })), (i & 1) == (j & 1));
			ASSERT_EQ(evaluator->getBoolState(Address({ 1, 2 })), (i & 2) == (j & 2));
			ASSERT_EQ(evaluator->getBoolState(Address({ 2, 2 })), (i & 4) == (j & 4));
			ASSERT_EQ(evaluator->getBoolState(Address({ 3, 2 })), (i & 8) == (j & 8));
			ASSERT_EQ(evaluator->getBoolState(Address({ 0, 3 })), i == j);
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
	ASSERT_EQ(evaluator->getState(Address({ 0, 0 })), logic_state_t::LOW);
	ASSERT_EQ(evaluator->getState(Address({ 1, 0 })), logic_state_t::LOW);
	ASSERT_EQ(evaluator->getState(Address({ 2, 0 })), logic_state_t::LOW);
	ASSERT_EQ(evaluator->getState(Address({ 3, 0 })), logic_state_t::LOW);
	ASSERT_EQ(evaluator->getState(Address({ 4, 0 })), logic_state_t::LOW);
	evaluator->setState(Address({ 0, 0 }), logic_state_t::HIGH);
	evaluator->tickStep(1);
	ASSERT_EQ(evaluator->getState(Address({ 0, 0 })), logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(Address({ 1, 0 })), logic_state_t::LOW);
	ASSERT_EQ(evaluator->getState(Address({ 2, 0 })), logic_state_t::UNDEFINED);
	ASSERT_EQ(evaluator->getState(Address({ 3, 0 })), logic_state_t::UNDEFINED);
	ASSERT_EQ(evaluator->getState(Address({ 4, 0 })), logic_state_t::UNDEFINED);
	evaluator->setState(Address({ 1, 0 }), logic_state_t::HIGH);
	evaluator->tickStep(1);
	ASSERT_EQ(evaluator->getState(Address({ 0, 0 })), logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(Address({ 1, 0 })), logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(Address({ 2, 0 })), logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(Address({ 3, 0 })), logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(Address({ 4, 0 })), logic_state_t::HIGH);
	evaluator->setState(Address({ 0, 0 }), logic_state_t::LOW);
	evaluator->tickStep(1);
	ASSERT_EQ(evaluator->getState(Address({ 0, 0 })), logic_state_t::LOW);
	ASSERT_EQ(evaluator->getState(Address({ 1, 0 })), logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(Address({ 2, 0 })), logic_state_t::UNDEFINED);
	ASSERT_EQ(evaluator->getState(Address({ 3, 0 })), logic_state_t::UNDEFINED);
	ASSERT_EQ(evaluator->getState(Address({ 4, 0 })), logic_state_t::UNDEFINED);
	circuit->tryRemoveBlock(Position { 0, 0 });
	ASSERT_EQ(evaluator->getState(Address({ 1, 0 })), logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(Address({ 2, 0 })), logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(Address({ 3, 0 })), logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(Address({ 4, 0 })), logic_state_t::UNDEFINED);
	evaluator->tickStep(1);
	ASSERT_EQ(evaluator->getState(Address({ 1, 0 })), logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(Address({ 2, 0 })), logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(Address({ 3, 0 })), logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(Address({ 4, 0 })), logic_state_t::HIGH);
	circuit->tryRemoveConnection(Position { 2, 0 }, Position { 3, 0 });
	ASSERT_EQ(evaluator->getState(Address({ 1, 0 })), logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(Address({ 2, 0 })), logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(Address({ 3, 0 })), logic_state_t::FLOATING);
	ASSERT_EQ(evaluator->getState(Address({ 4, 0 })), logic_state_t::HIGH);
	evaluator->tickStep(1);
	ASSERT_EQ(evaluator->getState(Address({ 1, 0 })), logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(Address({ 2, 0 })), logic_state_t::HIGH);
	ASSERT_EQ(evaluator->getState(Address({ 3, 0 })), logic_state_t::FLOATING);
	ASSERT_EQ(evaluator->getState(Address({ 4, 0 })), logic_state_t::UNDEFINED);
}
