#ifndef blockContainerTool_h
#define blockContainerTool_h

#include <QPainter>
#include <QColor>

#include "../effects/logicGridEffectDisplayer.h"
#include "backend/container/blockContainer.h"
class LogicGridWindow;


class BlockContainerTool {
public:
    inline BlockContainerTool(BlockContainer* blockContainer = nullptr) : blockContainer(blockContainer), selectedBlock(NONE), effectDisplayer() {}
    // This will also tell the tool to reset.
    inline void setBlockContainer(BlockContainer* blockContainer) {this->blockContainer = blockContainer; reset();}
    inline void selectBlock(BlockType selectedBlock) {this->selectedBlock = selectedBlock;}
    inline BlockType getSelectedBlock() const {return selectedBlock;}
    inline void display(QPainter& painter, const LogicGridWindow& gridWindow) {effectDisplayer.display(painter, gridWindow);}

    virtual void reset() {};
    virtual bool leftPress(const Position& pos) {return false;}
    virtual bool rightPress(const Position& pos) {return false;}
    virtual bool leftRelease(const Position& pos) {return false;}
    virtual bool rightRelease(const Position& pos) {return false;}
    virtual bool mouseMove(const Position& pos) {return false;}
    virtual bool enterBlockView(const Position& pos) {return false;}
    virtual bool exitBlockView(const Position& pos) {return false;}
    virtual bool keyPress(int keyId) {return false;}
    virtual bool keyRelease(int keyId) {return false;}

protected:
    BlockContainer* blockContainer;
    BlockType selectedBlock;
    LogicGridEffectDisplayer effectDisplayer;
};

#endif /* blockContainerTool_h */
