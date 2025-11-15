// Tutorial.h
#pragma once

#include <vector>
#include <string>
#include "TutorialState.h"
#include "gui/viewportManager/circuitView/circuitView.h"

class Tutorial {
public:
    Tutorial(CircuitView* view);

    void addState(const TutorialState& state);

    void start();           // load first state
    void nextState();       // move forward
    void previousState();   // move backward

    const TutorialState* getCurrentState() const;

private:
    void applyState(const TutorialState& state);

private:
    CircuitView* circuitView;
    std::vector<TutorialState> states;
    int currentIndex = -1;
};
;
