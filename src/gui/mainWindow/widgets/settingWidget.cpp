#include "settingWidget.h"

#include "SDL3/SDL_dialog.h"

#include "computerAPI/directoryManager.h"
#include "util/preprocessors.h"
#include "util/algorithm.h"
#include "../mainWindow.h"
#include "util/version.h"
#include "app.h"

#include "imgui/imgui_stdlib.h"
#include "imgui/imgui.h"

SettingWidget::SettingWidget(WidgetId widgetId, MainWindow& mainWindow) : Widget(widgetId, mainWindow) {
	setupGUIValue<std::string>("settingKeybind", "", nullptr);
	setupGUIValue<Keybind>("settingKeybindValue", Keybind(), nullptr);
}

struct SettingTreeNode {
	std::map<std::string, SettingTreeNode> children;
	bool leaf;
	void addPath(std::span<std::string> path) {
		if (path.size() == 0) {
			leaf = true;
			return;
		}
		children[path.front()].addPath(std::span(path.begin() + 1, path.end()));
	}
	void render(const std::string& path, const std::string& name, const std::string& settingKeybind, SettingWidget& settingWidget) const {
		if (leaf) {
			ImGui::PushID(path.c_str());
			ImGui::Indent(10);
			ImGui::SameLine();
			SettingType settingType = Settings::getType(path);
			if (!Settings::isDefalut(path)) {
				if (ImGui::Button("Reset")) {
					switch (settingType) {
					case SettingType::VOID: break;
					case SettingType::STRING: {
						App::runOnMain([this, path]() {
							const SettingTypeToType<SettingType::STRING>::type* defaultValue = Settings::getDefault<SettingType::STRING>(path);
							if (defaultValue) Settings::set<SettingType::STRING>(path, *defaultValue);
						});
					} break;
					case SettingType::INT: {
						App::runOnMain([this, path]() {
							const SettingTypeToType<SettingType::INT>::type* defaultValue = Settings::getDefault<SettingType::INT>(path);
							if (defaultValue) Settings::set<SettingType::INT>(path, *defaultValue);
						});
					} break;
					case SettingType::UINT: {
						App::runOnMain([this, path]() {
							const SettingTypeToType<SettingType::UINT>::type* defaultValue = Settings::getDefault<SettingType::UINT>(path);
							if (defaultValue) Settings::set<SettingType::UINT>(path, *defaultValue);
						});
					} break;
					case SettingType::DECIMAL: {
						App::runOnMain([this, path]() {
							const SettingTypeToType<SettingType::DECIMAL>::type* defaultValue = Settings::getDefault<SettingType::DECIMAL>(path);
							if (defaultValue) Settings::set<SettingType::DECIMAL>(path, *defaultValue);
						});
					} break;
					case SettingType::BOOL: {
						App::runOnMain([this, path]() {
							const SettingTypeToType<SettingType::BOOL>::type* defaultValue = Settings::getDefault<SettingType::BOOL>(path);
							if (defaultValue) Settings::set<SettingType::BOOL>(path, *defaultValue);
						});
					} break;
					case SettingType::KEYBIND: {
						App::runOnMain([this, path]() {
							const SettingTypeToType<SettingType::KEYBIND>::type* defaultValue = Settings::getDefault<SettingType::KEYBIND>(path);
							if (defaultValue) Settings::set<SettingType::KEYBIND>(path, *defaultValue);
						});
					} break;
					case SettingType::FILE_PATH: {
						App::runOnMain([this, path]() {
							const SettingTypeToType<SettingType::FILE_PATH>::type* defaultValue = Settings::getDefault<SettingType::FILE_PATH>(path);
							if (defaultValue) Settings::set<SettingType::FILE_PATH>(path, *defaultValue);
						});
					} break;
					}
				}
				ImGui::SameLine();
			}
			ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0, -2.5));
			switch (settingType) {
				case SettingType::VOID: {
					ImGui::Text("VOID SETTING TYPE");
				} break;
				case SettingType::STRING: {
					std::string str = valueOr(Settings::get<SettingType::STRING>(path), std::string(""));
					if (ImGui::InputText("##ID", &str)) {
						App::runOnMain([this, str, path]() {
							Settings::set<SettingType::STRING>(path, str);
						});
					}
				} break;
				case SettingType::INT: {
					int num = valueOr(Settings::get<SettingType::INT>(path), 0);
					if (ImGui::InputScalar("##ID", ImGuiDataType_S32, &num)) {
						App::runOnMain([this, num, path]() {
							Settings::set<SettingType::INT>(path, num);
						});
					}
				} break;
				case SettingType::UINT: {
					unsigned int num = valueOr(Settings::get<SettingType::UINT>(path), 0u);
					if (ImGui::InputScalar("##ID", ImGuiDataType_U32, &num)) {
						App::runOnMain([this, num, path]() {
							Settings::set<SettingType::UINT>(path, num);
						});
					}
				} break;
				case SettingType::DECIMAL: {
					double num = valueOr(Settings::get<SettingType::DECIMAL>(path), 0.0);
					if (ImGui::InputScalar("##ID", ImGuiDataType_Double, &num)) {
						App::runOnMain([this, num, path]() {
							Settings::set<SettingType::DECIMAL>(path, num);
						});
					}
				} break;
				case SettingType::BOOL: {
					bool boolean = valueOr(Settings::get<SettingType::BOOL>(path), false);
					if (ImGui::Checkbox("##ID", &boolean)) {
						App::runOnMain([this, boolean, path]() {
							Settings::set<SettingType::BOOL>(path, boolean);
						});
					}
				} break;
				case SettingType::KEYBIND: {
					Keybind keybind = (settingKeybind == path) ? (
						settingWidget.getGUIValue_rendering<Keybind>("settingKeybindValue")
					) : (
						valueOr(Settings::get<SettingType::KEYBIND>(path), Keybind())
					);
					if (ImGui::Button(keybind.toString().c_str())) {
						settingWidget.setGUIValue_rendering<std::string>("settingKeybind", path);
						settingWidget.setGUIValue_rendering<Keybind>("settingKeybindValue", Keybind());
					}
					// if (ImGui::InputText("##ID", k.c_str())) {
					// 	App::runOnMain([this, keybind, path]() {
					// 		Settings::set<SettingType::KEYBIND>(path, keybind);
					// 	});
					// }
				} break;
				case SettingType::FILE_PATH: {
					std::string filePath = valueOr(Settings::get<SettingType::FILE_PATH>(path), std::string(""));
					if (ImGui::Button("Select File")) {
						App::runOnMain([this, path, filePath]() {
							SDL_ShowOpenFileDialog([](void* userData, const char* const* filePaths, int filter) {
								App::runOnMain([&]() {
									if (!filePaths || !filePaths[0]) return;
									std::pair<SettingWidget*, std::string>* data = (std::pair<SettingWidget*, std::string>*)userData;
									std::string filePath = filePaths[0];
									if (filePath.empty()) return;
									Settings::set<SettingType::FILE_PATH>(data->second, DirectoryManager::shortenPath(filePath));
									delete data;
								});
							}, new std::pair(this, path), nullptr, nullptr, 0, filePath.c_str(), true);
						});
					}
					ImGui::SameLine();
					if (ImGui::InputText("##ID", &filePath)) {
						App::runOnMain([this, filePath, path]() {
							Settings::set<SettingType::FILE_PATH>(path, filePath);
						});
					}
				} break;
			}
			ImGui::Unindent(10);
			ImGui::PopID();
		} else {
			for (const auto& child : children) {
				if (child.second.leaf) {
					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImU32(0));
					ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImU32(0));
					ifGui (ImGui::TreeNodeEx((child.first + "##node").c_str(), ImGuiTreeNodeFlags_Leaf),
						ImGui::PopStyleColor(2);
					) {
						child.second.render(path.empty() ? child.first : (path + "/" + child.first), child.first, settingKeybind, settingWidget);
						ImGui::TreePop();
					}
				} else {
					if (ImGui::TreeNodeEx((child.first + "##node").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
						child.second.render(path.empty() ? child.first : (path + "/" + child.first), child.first, settingKeybind, settingWidget);
						ImGui::TreePop();
					}
				}
			}
		}
	}
};

void SettingWidget::render() {
	ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
	ImGui::SetNextWindowDockID(getMainWindow().getDockMainId(), ImGuiCond_FirstUseEver);
	getMainWindow().setNextWindowMainDockable();
	getMainWindow().pushWindowStyling();
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	bool open = true;
	ifGui (ImGui::Begin(("Setting###" + getWidgetIdStr()).c_str(), &open),
		getMainWindow().popWindowStyling();
	) {
		static ImGuiTextFilter filter;
		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
			filter.Clear();
		}
		ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
		filter.Draw("##Filter", -FLT_MIN);
		std::lock_guard mux(Settings::getSettingsMapReadLock());
		std::vector<std::string> paths;
		for (const auto& pair : Settings::getSettingsMap().getAllKeys())  {
			if (filter.PassFilter(pair.first.c_str())) {
				paths.push_back(pair.first);
			}
		}
		SettingTreeNode root;
		for (const std::string& path : paths) {
			std::vector<std::string> pathVec = stringSplit(path, '/');
			root.addPath(std::span(pathVec.begin(), pathVec.end()));
		}
		if (ImGui::BeginChild("tree")) {
			std::string settingKeybind = getGUIValue<std::string>("settingKeybind");
			root.render("", "", settingKeybind, *this);
		}
		ImGui::EndChild();
	}
	if (!open) {
		getMainWindow().destroyWidget(this->getWidgetId());
	}
	ImGui::End();
	ImGui::PopItemFlag();
}

void SettingWidget::update() {
	std::string settingKeybind = getGUIValue<std::string>("settingKeybind");
	if (settingKeybind.empty()) return;
	const std::set<ImGuiKey>& heldKeys = getMainWindow().getHeldKeys();
	if (heldKeys.contains(ImGuiKey_Escape)) {
		setGUIValue<std::string>("settingKeybind", "");
		return;
	}
	ImGuiKey keybind = Keybind().getKeybind();
	for (ImGuiKey key : heldKeys) {
		Keybind realKey(key);
		if (realKey.getKeybind() != ImGuiKey_Enter && !realKey.containsMods()) {
			keybind = realKey.getKeybind();
			break;
		}
	}
	assert(keybind != ImGuiKey_ReservedForModShift);
	if (heldKeys.contains(ImGuiKey::ImGuiMod_Ctrl)) keybind = (ImGuiKey)(keybind | ImGuiKey::ImGuiMod_Ctrl);
	if (heldKeys.contains(ImGuiKey::ImGuiMod_Shift)) keybind = (ImGuiKey)(keybind | ImGuiKey::ImGuiMod_Shift);
	if (heldKeys.contains(ImGuiKey::ImGuiMod_Alt)) keybind = (ImGuiKey)(keybind | ImGuiKey::ImGuiMod_Alt);
	if (heldKeys.contains(ImGuiKey::ImGuiMod_Super)) keybind = (ImGuiKey)(keybind | ImGuiKey::ImGuiMod_Super);
	setGUIValue<Keybind>("settingKeybindValue", Keybind(keybind));
	if (heldKeys.contains(ImGuiKey_Enter)) {
		Settings::set<SettingType::KEYBIND>(settingKeybind, keybind);
		setGUIValue<std::string>("settingKeybind", "");
	}
}
