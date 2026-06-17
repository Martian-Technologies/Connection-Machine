#include "feedbackWidget.h"

#include "app.h"
#include "gui/mainWindow/guiColors.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_stdlib.h"
#include "util/logsSanitizer.h"
#include "util/preprocessors.h"
#include "util/version.h"
#include "../mainWindow.h"

#include "network/network.h"
#include "util/compression.h"

FeedbackWidget::FeedbackWidget(WidgetId widgetId, MainWindow& mainWindow) : Widget(widgetId, mainWindow) { }

void FeedbackWidget::render() {
	getMainWindow().pushWindowStyling();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	constexpr float WINDOW_W = 440.0f;
	constexpr float WINDOW_H = 320.0f;
	ImGui::SetNextWindowPos((viewport->Size - ImVec2(WINDOW_W, WINDOW_H)) / 2, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(WINDOW_W, WINDOW_H), ImGuiCond_Always);

	bool open = true;
	ifGui (ImGui::Begin(("Feedback###" + getWidgetIdStr()).c_str(), &open,
		ImGuiWindowFlags_NoResize    |
		ImGuiWindowFlags_NoDocking   |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoTitleBar),
		getMainWindow().popWindowStyling();
		ImGui::PopStyleVar();
	) {
		// -- Title ------------------------------------------------------------
		{
			float curScale = ImGui::GetCurrentWindow()->FontWindowScale;
			ImGui::SetWindowFontScale(curScale * 1.4f);
			ImGui::Text("We'd love your feedback");
			ImGui::SetWindowFontScale(curScale);
		}
		ImGui::Spacing();

		// -- Hint text --------------------------------------------------------
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
		ImGui::TextWrapped("Tell us what's working well, what's broken, or what would make the experience smoother.");
		ImGui::PopStyleColor();
		ImGui::Spacing();

		// -- Textarea ---------------------------------------------------------
		const float reservedBelow = 28.0f /*checkbox row*/ + 8.0f /*spacing*/ + 24.0f /*buttons*/ + 16.0f /*bottom pad*/;
		const float textareaH = ImGui::GetContentRegionAvail().y - reservedBelow;

		ImGui::PushStyleColor(ImGuiCol_FrameBg, GUIColors::WIDGET_BACKGROUND_DARK);
		ImGui::InputTextMultiline(
			"##feedback_body",
			&m_feedbackText,
			ImVec2(-FLT_MIN, textareaH),
			m_submitted ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None
		);
		ImGui::PopStyleColor();
		ImGui::Spacing();

		// -- Include-state checkbox row ---------------------------------------
		ImGui::Checkbox("##include_state", &m_includeState);
		ImGui::SameLine();
		ImGui::Text("Include app state");
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
			ImGui::SetTooltip(
				"Shares a snapshot of your current logic graph and settings\n"
				"so we can reproduce the issue faster.\n"
				"No personal files are sent."
			);
		}
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
		ImGui::Text("(?)");
		ImGui::PopStyleColor();
		ImGui::Spacing();

		// -- Buttons ----------------------------------------------------------
		const float buttonW = 110.0f;
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		// Right-align the two buttons
		ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x - buttonW * 2 - spacing);

		if (ImGui::Button("Cancel", ImVec2(buttonW, 0))) {
			App::runOnMain([this]() {
				getMainWindow().destroyWidget(this->getWidgetId());
			});
		}
		ImGui::SameLine();

		// Dim the send button while empty or already submitted
		bool canSend = !m_feedbackText.empty() && !m_submitted;
		if (!canSend) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button(m_submitted ? "Sent!" : "Send Feedback", ImVec2(buttonW, 0))) {
			// Snapshot values before the lambda captures them
			std::string feedbackText  = m_feedbackText;
			bool        includeState  = m_includeState;

			m_submitted = true;
			m_submitAnimTimer = 0.0f;

			App::runOnMain([this, feedbackText, includeState]() {
				logInfo("Feedback submission initiated (chars={}, include_state={})",
					"FeedbackWidget::render", feedbackText.size(), includeState);

				std::vector<Network::Attachment> attachments;

				if (includeState) {
					// App-state attachment
					{
						Network::Attachment att;
						std::string appState = App::dumpState().dump();
						std::optional<std::string> compressed = compressString(appState);
						if (compressed) {
							att.data        = std::move(compressed.value());
							att.context     = "app_state.json.br";
							att.contentType = "application/x-brotli";
							logInfo("Attached compressed app state ({} bytes)", "FeedbackWidget::render", att.data.size());
						} else {
							att.data        = std::move(appState);
							att.context     = "app_state.json";
							att.contentType = "application/json";
							logInfo("Attached uncompressed app state ({} bytes)", "FeedbackWidget::render", att.data.size());
						}
						attachments.push_back(std::move(att));
					}

					// Logs attachment
					{
						logInfo("Logs will now be attached to feedback. no logs past this point", "FeedbackWidget::render");
						Network::Attachment att;
						std::string logs = sanitizeLogsForPaths(getLogContents());
						std::optional<std::string> compressed = compressString(logs);
						if (compressed) {
							att.data        = std::move(compressed.value());
							att.context     = "logs.txt.br";
							att.contentType = "application/x-brotli";
						} else {
							att.data        = std::move(logs);
							att.context     = "logs.txt";
							att.contentType = "text/plain";
						}
						attachments.push_back(std::move(att));
					}
				}

				logInfo("Feedback includes {} attachment(s)", "FeedbackWidget::render", attachments.size());
				Network::get().sendFeedback(
					getMainWindow(),
					"User Feedback from Connection Machine UI v" + getCurrentVersion().toString(),
					feedbackText,
					attachments
				);
				logInfo("Feedback submission dispatched to network API", "FeedbackWidget::render");
			});
		}
		if (!canSend) {
			ImGui::EndDisabled();
		}

		// Auto-close ~1.5 s after a successful send
		if (m_submitted && m_submitAnimTimer > 1.5f) {
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

void FeedbackWidget::update() {
	if (m_submitted) {
		m_submitAnimTimer += ImGui::GetIO().DeltaTime;
	}
}
