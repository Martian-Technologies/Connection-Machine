#include "popupWidget.h"

#include "app.h"
#include "util/preprocessors.h"
#include "../mainWindow.h"
#include "imgui/imgui_internal.h"

PopupWidget::PopupWidget(WidgetId widgetId, MainWindow& mainWindow, std::string message, const std::vector<std::pair<std::string, std::function<void()>>>& buttons) :
	Widget(widgetId, mainWindow), message(message), buttons(buttons) { }

void PopupWidget::render() {
	if (open > 1) {
		open--;
		return;
	}
	if (open == 1) {
		ImGui::OpenPopup(("###" + getWidgetIdStr()).c_str());
		open = 0;
	}
	getMainWindow().pushWindowStyling();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	ifGui (ImGui::BeginPopupModal(("###" + getWidgetIdStr()).c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar),
		getMainWindow().popWindowStyling();
		ImGui::PopStyleVar();
	) {
		ImGui::Text("%s", message.c_str());
		const ImVec2 labelSize = ImGui::CalcTextSize(message.c_str(), NULL, true);
		unsigned int itemsWidth = 0;
		for (const auto& button : buttons) {
			const ImVec2 labelSize = ImGui::CalcTextSize(button.first.c_str(), NULL, true);
			ImGuiStyle& style = ImGui::GetStyle();
			itemsWidth += ImGui::CalcItemSize(ImVec2(0, 0), labelSize.x + style.FramePadding.x * 2.0f, labelSize.y + style.FramePadding.y * 2.0f).x;
		}
		if (labelSize.x > itemsWidth) {
			ImGui::SetCursorPosX((labelSize.x - itemsWidth) / 2.0);
		}
		bool first = true;
		for (const auto& button : buttons) {
			if (first) {
				first = false;
			} else {
				ImGui::SameLine();
			}
			if (ImGui::Button(button.first.c_str())) {
				ImGui::CloseCurrentPopup();
				App::runOnMain([fn = button.second, this](){
					getMainWindow().destroyWidget(getWidgetId());
					if (fn) fn();
				});
			}
		}
		ImGui::EndPopup();
	}
}
