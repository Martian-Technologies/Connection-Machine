#include "circuitTestCase.h"

#include "backend/address.h"
#include "backend/blockData/blockData.h"
#include "backend/evaluator/evalDefs.h"
#include "backend/evaluator/evaluator.h"
#include "backend/evaluator/simulator/logicState.h"
#include "backend/position/position.h"


bool CircuitTestCase::runTest(BlockType blockType, bool haltOnFailure, Environment& environment) {
    /*testCommands = {// 0 0 Z
                    TestCommand{SET_STATES, 0, {std::make_pair("Input", (logic_state_t)0), std::make_pair("Enable", (logic_state_t)0)}},
                    TestCommand(TICK_STEP, 10),
                    TestCommand(CHECK_STATES, 0, {std::make_pair("Output", (logic_state_t)2)}),
                    // 0 1 0
                    TestCommand{SET_STATES, 0, {std::make_pair("Input", (logic_state_t)1), std::make_pair("Enable", (logic_state_t)0)}},
                    TestCommand(TICK_STEP, 10),
                    TestCommand(CHECK_STATES, 0, {std::make_pair("Output", (logic_state_t)0)}),
                    // 0 Z X
                    TestCommand{SET_STATES, 0, {std::make_pair("Input", (logic_state_t)2), std::make_pair("Enable", (logic_state_t)0)}},
                    TestCommand(TICK_STEP, 10),
                    TestCommand(CHECK_STATES, 0, {std::make_pair("Output", (logic_state_t)3)}),
                    // 0 X X
                    TestCommand{SET_STATES, 0, {std::make_pair("Input", (logic_state_t)3), std::make_pair("Enable", (logic_state_t)0)}},
                    TestCommand(TICK_STEP, 10),
                    TestCommand(CHECK_STATES, 0, {std::make_pair("Output", (logic_state_t)3)}),
                    // 1 0 Z
                    TestCommand{SET_STATES, 0, {std::make_pair("Input", (logic_state_t)0), std::make_pair("Enable", (logic_state_t)1)}},
                    TestCommand(TICK_STEP, 10),
                    TestCommand(CHECK_STATES, 0, {std::make_pair("Output", (logic_state_t)2)}),
                    // 1 1 1
                    TestCommand{SET_STATES, 0, {std::make_pair("Input", (logic_state_t)1), std::make_pair("Enable", (logic_state_t)1)}},
                    TestCommand(TICK_STEP, 10),
                    TestCommand(CHECK_STATES, 0, {std::make_pair("Output", (logic_state_t)1)}),
                    // 1 Z X
                    TestCommand{SET_STATES, 0, {std::make_pair("Input", (logic_state_t)2), std::make_pair("Enable", (logic_state_t)1)}},
                    TestCommand(TICK_STEP, 10),
                    TestCommand(CHECK_STATES, 0, {std::make_pair("Output", (logic_state_t)3)}),
                    // 1 X X
                    TestCommand{SET_STATES, 0, {std::make_pair("Input", (logic_state_t)3), std::make_pair("Enable", (logic_state_t)1)}},
                    TestCommand(TICK_STEP, 10),
                    TestCommand(CHECK_STATES, 0, {std::make_pair("Output", (logic_state_t)3)}),
                    // Z 0 Z
                    TestCommand{SET_STATES, 0, {std::make_pair("Input", (logic_state_t)0), std::make_pair("Enable", (logic_state_t)2)}},
                    TestCommand(TICK_STEP, 10),
                    TestCommand(CHECK_STATES, 0, {std::make_pair("Output", (logic_state_t)2)}),
                    // Z 1 X
                    TestCommand{SET_STATES, 0, {std::make_pair("Input", (logic_state_t)1), std::make_pair("Enable", (logic_state_t)2)}},
                    TestCommand(TICK_STEP, 10),
                    TestCommand(CHECK_STATES, 0, {std::make_pair("Output", (logic_state_t)3)}),
                    // Z Z X
                    TestCommand{SET_STATES, 0, {std::make_pair("Input", (logic_state_t)2), std::make_pair("Enable", (logic_state_t)2)}},
                    TestCommand(TICK_STEP, 10),
                    TestCommand(CHECK_STATES, 0, {std::make_pair("Output", (logic_state_t)3)}),
                    // Z X X
                    TestCommand{SET_STATES, 0, {std::make_pair("Input", (logic_state_t)3), std::make_pair("Enable", (logic_state_t)2)}},
                    TestCommand(TICK_STEP, 10),
                    TestCommand(CHECK_STATES, 0, {std::make_pair("Output", (logic_state_t)3)}),
                    // X 0 Z
                    TestCommand{SET_STATES, 0, {std::make_pair("Input", (logic_state_t)0), std::make_pair("Enable", (logic_state_t)3)}},
                    TestCommand(TICK_STEP, 10),
                    TestCommand(CHECK_STATES, 0, {std::make_pair("Output", (logic_state_t)2)}),
                    // X 1 X
                    TestCommand{SET_STATES, 0, {std::make_pair("Input", (logic_state_t)1), std::make_pair("Enable", (logic_state_t)3)}},
                    TestCommand(TICK_STEP, 10),
                    TestCommand(CHECK_STATES, 0, {std::make_pair("Output", (logic_state_t)3)}),
                    // X Z X
                    TestCommand{SET_STATES, 0, {std::make_pair("Input", (logic_state_t)2), std::make_pair("Enable", (logic_state_t)3)}},
                    TestCommand(TICK_STEP, 10),
                    TestCommand(CHECK_STATES, 0, {std::make_pair("Output", (logic_state_t)3)}),
                    // X X X
                    TestCommand{SET_STATES, 0, {std::make_pair("Input", (logic_state_t)3), std::make_pair("Enable", (logic_state_t)3)}},
                    TestCommand(TICK_STEP, 10),
                    TestCommand(CHECK_STATES, 0, {std::make_pair("Output", (logic_state_t)3)})
                };*/

    // // retrieve necessary objects to run test
    // bool fullTestSucceedStatus = true;
    // Backend& backend = environment.getBackend();
    // CircuitManager& cirManager = backend.getCircuitManager();
    // const EvaluatorManager& evalManager = backend.getEvaluatorManager();
    // BlockDataManager& blockDataManager = backend.getBlockDataManager();

    // circuit_id_t cirId = cirManager.createNewCircuit(false);
    // SharedCircuit cir = cirManager.getCircuit(cirId);
    // evaluator_id_t evalId = backend.createEvaluator(cirId).value();
    // const SharedEvaluator eval = evalManager.getEvaluator(evalId);
    // eval->setPause(true);
    // const BlockContainer blockContainer = cir->getBlockContainer();

    // const BlockData* blockData = blockDataManager.getBlockData(blockType);
    // std::unordered_map<connection_end_id_t, BlockData::ConnectionData> connections = blockData->getConnections();
    // // change implementation of this when BlockData::getConnectionNameToId is implemented
    // NamePositionMap nameToConnectedBlockPosition;

    // // generate the test circuit
    // if (!cir->tryInsertBlock(Position(0,0), Orientation(), blockType)) {
    //     logError("Couldn't insert test circuit block {}", "circuitTestCase", "blockType");
    //     return false;
    // }

    // for (auto iter = connections.begin(); iter != connections.end(); iter++) {
    //     if (iter->second.portType == BlockData::ConnectionData::PortType::INPUT || iter->second.portType == BlockData::ConnectionData::PortType::BIDIRECTIONAL) {
    //         Position internalConnPos = Position(iter->second.positionOnBlock.dx, iter->second.positionOnBlock.dy);
    //         Position externalConnPos = Position(-1-nameToConnectedBlockPosition.size(), 0);
    //         if (!cir->tryInsertBlock(externalConnPos, Orientation(), SWITCH)) {
    //             logError("Couldn't insert test circuit block", "circuitTestCase");
    //             return false;
    //         }
    //         if (!cir->tryCreateConnection(externalConnPos, internalConnPos)) {
    //             logError("Couldn't create test circuit connection", "circuitTestCase");
    //             return false;
    //         }
    //         nameToConnectedBlockPosition.insert({blockData->getConnectionIdToName(iter->first).value(), externalConnPos});
    //     }
    //     if (iter->second.portType == BlockData::ConnectionData::PortType::OUTPUT || iter->second.portType == BlockData::ConnectionData::PortType::BIDIRECTIONAL) {
    //         Position internalConnPos = Position(iter->second.positionOnBlock.dx, iter->second.positionOnBlock.dy);
    //         Position externalConnPos = Position(-1-nameToConnectedBlockPosition.size(), 0);
    //         if (!cir->tryInsertBlock(externalConnPos, Orientation(), LIGHT)) {
    //             logError("Couldn't insert test circuit block", "circuitTestCase");
    //             return false;
    //         }
    //         if (!cir->tryCreateConnection(internalConnPos, externalConnPos)) {
    //             logError("Couldn't create test circuit connection", "circuitTestCase");
    //             return false;
    //         }
    //         nameToConnectedBlockPosition.insert({blockData->getConnectionIdToName(iter->first).value(), externalConnPos});
    //     }
    // }

    // // run tests on the generated test circuit
    // for (auto commandIter = testCommands.begin(); commandIter != testCommands.end(); commandIter++) {
    //     logInfo("Performing a test command of type {}", "CircuitTestCase", getTestCommandTypeString(commandIter->type));
    //     bool isTestGroupSuccessful = true;
    //     if (commandIter->type == NOP_COMMAND) {
    //         continue;
    //     } else if (commandIter->type == SET_STATES) {
    //         runSetStatesCommand(*commandIter, eval, nameToConnectedBlockPosition);
    //     } else if (commandIter->type == CHECK_STATES) {
    //         isTestGroupSuccessful = runCheckStatesCommand(*commandIter, eval, nameToConnectedBlockPosition);
    //     } else if (commandIter->type == TICK_STEP) {
    //         logInfo("Stepping forward by {} ticks", "CircuitTestCase - TICK_STEP", commandIter->ticks);
    //         eval->tickStep(commandIter->ticks);
    //     } else {
    //         logError("Unrecognized test command", "CircuitTestCase");
    //         isTestGroupSuccessful = false;
    //     }

    //     if (!isTestGroupSuccessful) {
    //         logError("Test group failed.", "CircuitTestCase");
    //         fullTestSucceedStatus = false;
    //         if (haltOnFailure) {
    //             return false;
    //         }
    //     }
    // }
    // return fullTestSucceedStatus;
}

void CircuitTestCase::runSetStatesCommand(TestCommand testCommand, const SharedEvaluator eval, NamePositionMap& nameToConnectedBlockPosition) {
    for (auto statesIter = testCommand.states.begin(); statesIter != testCommand.states.end(); statesIter++) {
        auto blockPosIter = nameToConnectedBlockPosition.find(statesIter->first);
        eval->setState((Address(blockPosIter->second)), statesIter->second);
        logInfo("Set port '{}' to state '{}'", "CircuitTestCase - SET_STATES", statesIter->first, statesIter->second);
    }
}

bool CircuitTestCase::runCheckStatesCommand(TestCommand testCommand, const SharedEvaluator eval, NamePositionMap& nameToConnectedBlockPosition) {
// probably needs to return the state of every single block tested instead of whether just one of them fails
    bool testSucceeded = true;
    for (auto statesIter = testCommand.states.begin(); statesIter != testCommand.states.end(); statesIter++) {
        auto blockPosIter = nameToConnectedBlockPosition.find(statesIter->first);
        logic_state_t actualState = eval->getState((Address(blockPosIter->second)));
        if (actualState != statesIter->second) {
            testSucceeded = false;
            logError("Inner test case failed: Expected port '{}' to have output '{}', got '{}'", "CircuitTestCase - CHECK_STATES", statesIter->first, statesIter->second, actualState);
        } else {
            logInfo("Inner test case succeeded: Expected port '{}' to have output '{}', got '{}'", "CircuitTestCase - CHECK_STATES", statesIter->first, statesIter->second, actualState);
        }
    }
    return testSucceeded;
}


/*
1. Test if a block type behaves in the way the test expects
2. The tested circuit will be abstracted into a block that we run tests on through the ports.

1. Make a circuit, place the abstracted block at (0,0), run tests on it
2. To get connection end ID from port name, use connectionIdNames map from blockData
3. To get port position, use getConnectionVector
4. Create eval for circuit if not already there
5. Create things (switch & lights) connected to ports of the circuit block to evaluate it
6. Create connections with circuit createConnection
7. Use stuff in circuit.h to create blocks (tryInsertBlock, tryCreateConnection)
8. Create new circuit using circuitManager createNewCircuit
*/