#ifndef feedbackWidget_h
#define feedbackWidget_h

#include "../widget.h"
#include <string>

class FeedbackWidget : public Widget {
public:
	FeedbackWidget(WidgetId widgetId, MainWindow& mainWindow);

private:
	void render() override final;
	void update() override final;

	std::string m_feedbackText;
	bool m_includeState = false;
	bool m_submitted = false;
	float m_submitAnimTimer = 0.0f;
};

#endif /* feedbackWidget_h */
