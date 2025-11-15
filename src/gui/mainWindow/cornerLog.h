#ifndef cornerLog_h
#define cornerLog_h

namespace Rml {
	class ElementDocument;
	class Element;
}

class CornerLog {
public:
	CornerLog(Rml::ElementDocument* document);

	void updateMessages();

	void log(const std::string& message);
	template<typename ...Args>
	void log(const fmt::format_string<Args...>& formatString, Args&&...args) {
		std::ostringstream message;
		message << fmt::format(formatString, std::forward<Args>(args)...);
		log(message.str());
	}

	void logError(const std::string& message);
	template<typename ...Args>
	void logError(const fmt::format_string<Args...>& formatString, Args&&...args) {
		std::ostringstream message;
		message << fmt::format(formatString, std::forward<Args>(args)...);
		logError(message.str());
	}

private:
	Rml::ElementDocument* document;
	Rml::Element* cornerLogElement;

	float timeout = 4.f;
	std::list<std::chrono::time_point<std::chrono::system_clock>> messageCreationTime;
};

#endif /* cornerLog_h */
