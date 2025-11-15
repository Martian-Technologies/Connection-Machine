#ifndef network_h
#define network_h

#include "computerAPI/kvStore.h"

class PopUpManager;

class Network {
public:
	static Network& get();
	static void kill();

	struct Attachment {
		std::string context;
		std::string contentType;
		std::string data;
	};

	bool checkConnectedToInternet();
	void sendFeedback(PopUpManager& popUpManager, const std::string& title, const std::string& description, const std::vector<Attachment>& attachments);
	void sendAttachments(PopUpManager& popUpManager, const std::vector<Attachment>& attachments, std::string reportUuid);
	void checkForUpdates(PopUpManager& popUpManager);

	// do not call
	Network();
	~Network();

private:
	static std::optional<Network> singletonInstance;

	static inline std::string urlEncode(const std::string& s) {
		static const char hex[] = "0123456789ABCDEF";
		std::string out;
		out.reserve(s.size() * 3);
		for (unsigned char c : s) {
			const bool unreserved =
				(c >= 'A' && c <= 'Z') ||
				(c >= 'a' && c <= 'z') ||
				(c >= '0' && c <= '9') ||
				c == '-' || c == '_' || c == '.' || c == '~';
			if (unreserved) {
				out.push_back(static_cast<char>(c));
			} else {
				out.push_back('%');
				out.push_back(hex[c >> 4]);
				out.push_back(hex[c & 0x0F]);
			}
		}
		return out;
	}

	std::shared_ptr<KVStore> kvStore;
};

#endif /* network_h */
