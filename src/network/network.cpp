#include "network.h"
#include "gui/mainWindow/popUps/popUpManager.h"
#include "util/version.h"
#include <httplib.h>

std::optional<Network> Network::singletonInstance;

Network& Network::get() {
	if (!singletonInstance.has_value()) {
		singletonInstance.emplace();
	}
	return singletonInstance.value();
}
void Network::kill() {
	singletonInstance.reset();
}

Network::Network() : kvStore(KVStore::getStore("network")) {
}

Network::~Network() {
}

bool Network::checkConnectedToInternet() {
	httplib::SSLClient cli("www.gstatic.com");
	cli.set_connection_timeout(5, 0);
	cli.set_read_timeout(10, 0);
	cli.enable_server_certificate_verification(true);
	auto res = cli.Get("/generate_204");
	if (res && res->status == 204) {
		return true;
	}
	return false;
}

void Network::sendFeedback(PopUpManager& popUpManager, const std::string& title, const std::string& description, const std::vector<Network::Attachment>& attachments) {
	std::thread t([this, &popUpManager, title, description, attachments]() {
		httplib::SSLClient cli("api.connection-machine.com");
		cli.set_read_timeout(10, 0);
		cli.set_connection_timeout(5, 0);
		cli.enable_server_certificate_verification(true);

		const nlohmann::json jsonData = {
			{"title", title},
			{"description", description}
		};

		std::string body = jsonData.dump();

		logInfo("Sending feedback: {}", "Network", body);

		httplib::Result res = cli.Post("/api/reports", body, "application/json");
		if (res) {
			logInfo("Feedback response content: {}", "Network", res->body);
		}
		if (res && res->status / 100 != 2) {
			if (Network::get().checkConnectedToInternet()) {
				popUpManager.addOptionsPopUp("Feedback sending failed.", { {"OK", []() {}} }, true);
			} else {
				popUpManager.addOptionsPopUp("No internet connection.", { {"OK", []() {}} }, true);
			}
			return;
		}
		if (attachments.empty()) {
			popUpManager.addOptionsPopUp("Feedback submitted successfully!", { {"OK", []() {}} }, true);
			return;
		}
		sendAttachments(popUpManager, attachments, nlohmann::json::parse(res->body)["uuid"].get<std::string>());
	});
	t.detach();
	logInfo("Feedback thread dispatched", "Network");
}

void Network::sendAttachments(PopUpManager& popUpManager, const std::vector<Network::Attachment>& attachments, std::string reportUuid) {
	std::thread t([this, &popUpManager, attachments, reportUuid]() {
		httplib::SSLClient cli("api.connection-machine.com");
		cli.set_read_timeout(10, 0);
		cli.set_connection_timeout(5, 0);
		cli.enable_server_certificate_verification(true);

		std::vector<Attachment> attachmentsFailed;

		for (const auto& attachment : attachments) {
			const std::string content_type = attachment.contentType.empty() ? "application/octet-stream" : attachment.contentType;
			std::string path = "/api/reports/";
			path += reportUuid;
			path += "/attachments?context=";
			path += urlEncode(attachment.context);
			logInfo("Uploading attachment with context \"{}\" ({} bytes) to {}", "Network", attachment.context, attachment.data.size(), path);
			int kMaxAttempts = 2;
			bool success = false;
			for (int attempt = 1; attempt <= kMaxAttempts; ++attempt) {
				httplib::Result res = cli.Post(path.c_str(), attachment.data, content_type.c_str());
				logInfo("Attachment upload response: status={}, body(size={})", "Network",
					res ? std::to_string(res->status) : "no response",
					res ? std::to_string(res->body.size()) : "0");
				if (res && res->status / 100 == 2) {
					success = true;
					break;
				} else {
					logError("Attachment upload attempt {}/{} failed for context '{}'", "Network", attempt, kMaxAttempts, attachment.context);
				}
			}
			if (!success) {
				attachmentsFailed.push_back(attachment);
			}
		}
		if (!attachmentsFailed.empty()) {
			popUpManager.addOptionsPopUp("Some or all attachments failed to upload.", { {"OK", []() {}} }, true);
		} else {
			popUpManager.addOptionsPopUp("Feedback submitted successfully!", { {"OK", []() {}} }, true);
		}
	});
	t.detach();
}

void Network::checkForUpdates(PopUpManager& popUpManager) {
	std::thread t([this, &popUpManager]() {
		Version currentVersion = getCurrentVersion();
		logInfo("Checking for updates, current version: {}", "Network", currentVersion.toString());
		std::string ignoredVersionStr = kvStore->get<KVType::STRING>("ignored_version").value_or("0.0.0");
		Version ignoredVersion = parseVersionString(ignoredVersionStr);
		logInfo("Minimum version to notify greater than: {}", "Network", ignoredVersion.toString());
		httplib::SSLClient cli("api.github.com");
		cli.set_read_timeout(10, 0);
		cli.set_connection_timeout(5, 0);
		cli.enable_server_certificate_verification(true);
		httplib::Headers headers = {
			{"Accept", "application/vnd.github+json"},
			{"X-GitHub-Api-Version", "2022-11-28"},
			{"User-Agent", "Connection-Machine-App"}
		};
		httplib::Result res = cli.Get("/repos/Martian-Technologies/Connection-Machine/releases?per_page=1", headers);
		if (res) {
			// logInfo("Update check response content: {}", "Network", res->body);
			if (res->status / 100 == 2) {
				try {
					auto jsonResponse = nlohmann::json::parse(res->body);
					if (!jsonResponse.is_array() || jsonResponse.size() == 0) {
						logError("Unexpected update check response format", "Network");
						return;
					}
					auto latestRelease = jsonResponse[0];
					std::string latestVersionStr = latestRelease["tag_name"].get<std::string>();
					Version latestVersion = parseVersionString(latestVersionStr);
					if (latestVersion <= ignoredVersion) {
						logInfo("Latest version {} is less than or equal to ignored version {}, not notifying", "Network", latestVersion.toString(), ignoredVersion.toString());
						return;
					}
					logInfo("Latest version from update check: {}", "Network", latestVersion.toString());
					if (latestVersion <= currentVersion) {
						logInfo("No update available", "Network");
						return;
					}
					std::string body = latestRelease["body"].get<std::string>();
					std::string message = "A new version of Connection Machine is available!";
					std::string smallMessage = "Current version: " + currentVersion.toString() + "\n";
					smallMessage += "Latest version: " + latestVersion.toString() + "\n";
					smallMessage += "Changes:\n" + body;
					popUpManager.addOptionsPopUp(message, smallMessage, { {"OK", []() {}}, {"Ignore Version", [this, latestVersion]() {
						kvStore->set<KVType::STRING>("ignored_version", latestVersion.toString());
					}} }, true);
				} catch (const nlohmann::json::parse_error& e) {
					logError("Failed to parse update check response JSON: {}", "Network", e.what());
					return;
				}
			}
		}
	});
	t.detach();
	logInfo("Update check thread dispatched", "Network");
}
