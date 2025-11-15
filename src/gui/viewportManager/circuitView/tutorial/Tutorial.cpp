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
