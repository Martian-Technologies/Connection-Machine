#include "Tutorial.h"
#include "backend/circuit/circuit.h"

Tutorial::Tutorial(CircuitView* view)
    : circuitView(view) {}

void Tutorial::addState(const TutorialState& state) {
    states.push_back(state);
}

void Tutorial::start() {
    if (states.empty()) return;
    currentIndex = 0;
    applyState(states[currentIndex]);
}

void Tutorial::nextState() {
    if (currentIndex + 1 < (int)states.size()) {
        currentIndex++;
        applyState(states[currentIndex]);
    }
}

void Tutorial::previousState() {
    if (currentIndex - 1 >= 0) {
        currentIndex--;
        applyState(states[currentIndex]);
    }
}

TutorialState* Tutorial::getCurrentState() const {
    if (currentIndex < 0 || currentIndex >= (int)states.size()) return nullptr;
    return &states[currentIndex];
}


void Tutorial::applyState(const TutorialState& state) {
    SharedCircuit circuit = circuitView->getCircuit();
    if (!circuit) return;
    circuit->clearCircuit();
    for (const auto& inst : state.blocks) {
        circuit->tryInsertBlock(inst.position, inst.orientation, inst.type);
    }
    circuitView->getEventRegister().doEvent(
        Event("status bar changed", state.name)
    );
}

