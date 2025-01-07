#ifndef moveTool_h
#define moveTool_h

#include "blockContainerTool.h"
#include "middleEnd/selection.h"

class MoveTool : public BlockContainerTool {
public:
    inline MoveTool() : BlockContainerTool(), stage('o'), originSelection(nullptr), tensorStage(-1) {}

    inline void reset() override final {
        stage = 'o';
        originSelection = nullptr;
        tensorStage = -1;
        updateElements();
    }

    void initialize(ToolManagerEventRegister& toolManagerEventRegister) override final {
        BlockContainerTool::initialize(toolManagerEventRegister);
        toolManagerEventRegister.registerFunction("tool primary activate", std::bind(&MoveTool::click, this, std::placeholders::_1));
        toolManagerEventRegister.registerFunction("tool secondary activate", std::bind(&MoveTool::unclick, this, std::placeholders::_1));
        toolManagerEventRegister.registerFunction("pointer move", std::bind(&MoveTool::pointerMove, this, std::placeholders::_1));
        // toolManagerEventRegister.registerFunction("pointer enter view", std::bind(&MoveTool::enterBlockView, this, std::placeholders::_1));
        // toolManagerEventRegister.registerFunction("pointer exit view", std::bind(&MoveTool::exitBlockView, this, std::placeholders::_1));
        toolManagerEventRegister.registerFunction("tool rotate block cw", std::bind(&MoveTool::confirm, this, std::placeholders::_1));
    }

    bool click(const Event* event);
    bool unclick(const Event* event);
    bool confirm(const Event* event);
    bool pointerMove(const Event* event);

    // bool startPlaceBlock(const Event* event);
    // bool stopPlaceBlock(const Event* event);
    // bool startDeleteBlocks(const Event* event);
    // bool stopDeleteBlocks(const Event* event);
    // bool enterBlockView(const Event* event);
    // bool exitBlockView(const Event* event);

private:
    void updateElements();

    char stage;

    FPosition pointer;
    
    // blocks
    Position originPosition;
    SharedSelection originSelection;

    // tensor stuff
    Position step;
    int tensorStage;

    // destination
    Position destination;
};

#endif /* moveTool_h */
