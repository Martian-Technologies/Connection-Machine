#ifndef pasteTool_h
#define pasteTool_h

#include "../circuitTool.h"

class Backend;

class PasteTool : public CircuitTool {
public:
	void activate() override final;

	static inline std::vector<std::string> getModes_() { return {}; }
	static inline std::string getPath_() { return "paste tool"; }
	inline std::string getPath() const override final { return getPath_(); }
	inline unsigned int getStackId() const override final { return 1; }

	void updateElements() override final;

	bool rotateCW(const Event* event);
	bool rotateCCW(const Event* event);
	bool place(const Event* event);

private:
	Position lastElementPosition;
	ElementId elementId = 0;
	unsigned long long lastClipboardEditCounter = 0;
	Orientation transformAmount = Orientation();
	bool validatePlacement() const;
};

#endif /* pasteTool_h */
