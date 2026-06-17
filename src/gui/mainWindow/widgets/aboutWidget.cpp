#include "aboutWidget.h"

#include "app.h"
#include "computerAPI/directoryManager.h"
#include "gui/mainWindow/guiColors.h"
#include "imgui/imgui_internal.h"
#include "util/preprocessors.h"
#include "../mainWindow.h"
#include "util/version.h"

AboutWidget::AboutWidget(WidgetId widgetId, MainWindow& mainWindow) : Widget(widgetId, mainWindow) { }

void AboutWidget::render() {
	getMainWindow().pushWindowStyling();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos((viewport->Size - ImVec2(400, 370)) / 2, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(400, 370));
	bool open = true;
	ifGui (ImGui::Begin(("About###" + getWidgetIdStr()).c_str(), &open,
		ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse),
		getMainWindow().popWindowStyling();
		ImGui::PopStyleVar();
	) {
		if (ImGui::BeginChild("Title", ImVec2(0, 60))) {
			if (ImGui::BeginChild("Title", ImVec2(-70, 50))) {
				float curScale = ImGui::GetCurrentWindow()->FontWindowScale;
				ImGui::SetWindowFontScale(curScale * 2.2);
				ImGui::Text("Connection Machine");
				ImGui::SetWindowFontScale(curScale);
				ImGui::Text("%s", PROJECT_VERSION);
			}
			ImGui::EndChild();
			ImGui::SameLine();
			VkDescriptorSet descriptorSet = MainRenderer::get().getImage((DirectoryManager::getResourceDirectory() / "gateIcon.png").string());
			if (descriptorSet != VK_NULL_HANDLE) {
				ImGui::Image(descriptorSet, ImVec2(60, 60));
			} else {
				ImGui::Text("RENDERING BROKEN!! :(");
			}
		}
		ImGui::EndChild();
		ImGui::PushStyleColor(ImGuiCol_ChildBg, GUIColors::WIDGET_BACKGROUND_DARK);
		if (ImGui::BeginChild("Description", ImVec2(250, -20), ImGuiChildFlags_AlwaysUseWindowPadding)) {
			float curScale = ImGui::GetCurrentWindow()->FontWindowScale;
			ImGui::SetWindowFontScale(curScale * 1.2);
			ImGui::TextWrapped("Connection Machine is an open source project for designing and simulating logic systems.");
			ImGui::SetWindowFontScale(curScale);
		}
		ImGui::PopStyleColor();
		ImGui::EndChild();
		ImGui::SameLine();
		if (ImGui::BeginChild("Contributors", ImVec2(0, -20)), ImGuiChildFlags_Borders) {
			std::vector<std::string> contributors = {
				"Ben Herman",
				"Nikita Lurye",
				"Jack Jamison",
				"Connor Kostiew",
				"Gregory Lemonnier",
				"James P",
				"Sam C",
				"Alek Krupka",
				"August Bernberg",
				"Nicholas Ciuica",
				"Matthew Durcan",
				"Tiffany Cheng",
				"Patrick Chen",
				"Nathan Chen",
				"Dante Luis"
			};
			for (const auto& name : contributors) {
				ImGui::Text("%s", name.c_str());
			}
		}
		ImGui::EndChild();
		ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((400 - 50) / 2, 0));
		if (ImGui::Button("Close", ImVec2(50, 0))) {
			App::runOnMain([this]() {
				getMainWindow().destroyWidget(this->getWidgetId());
			});
		}
		ImGui::End();
	}
	if (!open) {
		getMainWindow().destroyWidget(this->getWidgetId());
	}
}
