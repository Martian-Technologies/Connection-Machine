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

};
