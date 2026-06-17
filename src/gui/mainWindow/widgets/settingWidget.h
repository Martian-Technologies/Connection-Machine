#ifndef settingWidget_h
#define settingWidget_h

#include "../widget.h"

class SettingWidget : public Widget {
	friend struct SettingTreeNode;
public:
	SettingWidget(WidgetId widgetId, MainWindow& mainWindow);

private:
	void render() override final;
	void update() override final;
};

#endif /* settingWidget_h */
