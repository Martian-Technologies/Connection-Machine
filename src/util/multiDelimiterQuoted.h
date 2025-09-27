#ifndef multiDelimiterQuoted_h
#define multiDelimiterQuoted_h

template <class CharT, class Traits = std::char_traits<CharT>>
class MultiDelimiterQuoted {
public:
    using string_type = std::basic_string<CharT, Traits>;

    explicit MultiDelimiterQuoted(
        string_type& str,
        std::vector<CharT> delims = { '"', '\'' },
        std::vector<CharT> escapes = { '\\' }
    )
        : s(str), delimiters(std::move(delims)), escapes(std::move(escapes)),
          used_delim(CharT()), used_escape(CharT()) {}

    // --- Inserter ---
    friend std::basic_ostream<CharT, Traits>&
    operator<<(std::basic_ostream<CharT, Traits>& os, const MultiDelimiterQuoted& multiDelimiterQuoted) {
        CharT delim = multiDelimiterQuoted.used_delim ? multiDelimiterQuoted.used_delim
                                    : (multiDelimiterQuoted.delimiters.empty() ? CharT('"') : multiDelimiterQuoted.delimiters.front());
        CharT escape = multiDelimiterQuoted.used_escape ? multiDelimiterQuoted.used_escape
                                      : (multiDelimiterQuoted.escapes.empty() ? CharT('\\') : multiDelimiterQuoted.escapes.front());

        os << delim;
        for (CharT c : multiDelimiterQuoted.s) {
            if (std::find(multiDelimiterQuoted.delimiters.begin(), multiDelimiterQuoted.delimiters.end(), c) != multiDelimiterQuoted.delimiters.end() ||
                std::find(multiDelimiterQuoted.escapes.begin(), multiDelimiterQuoted.escapes.end(), c) != multiDelimiterQuoted.escapes.end()) {
                os << escape;
            }
            os << c;
        }
        os << delim;
        return os;
    }

    // --- Extractor ---
    friend std::basic_istream<CharT, Traits>&
    operator>>(std::basic_istream<CharT, Traits>& is, MultiDelimiterQuoted multiDelimiterQuoted) {
        using istream_type = std::basic_istream<CharT, Traits>;
        auto& s = multiDelimiterQuoted.s;
        s.clear();

        typename istream_type::sentry sentry(is, true);
        if (!sentry) return is;

        CharT ch;
        if (!(is >> ch)) return is;

        // Case: not starting with delimiter
        if (std::find(multiDelimiterQuoted.delimiters.begin(), multiDelimiterQuoted.delimiters.end(), ch) == multiDelimiterQuoted.delimiters.end()) {
            if (!is.putback(ch)) { // safer than unget()
                is.setstate(std::ios::failbit);
                return is;
            }
            is >> s;
            return is;
        }

        // Quoted case
        multiDelimiterQuoted.used_delim = ch; // remember actual delimiter

        // Save skipws and disable temporarily
        struct SkipGuard {
            std::basic_ios<CharT, Traits>& ios;
            bool old;
            SkipGuard(std::basic_ios<CharT, Traits>& ios) : ios(ios), old(ios.flags() & std::ios::skipws) {
                ios.unsetf(std::ios::skipws);
            }
            ~SkipGuard() {
                if (old) ios.setf(std::ios::skipws);
            }
        } guard(is);

        while (is.get(ch)) {
            // Handle escape
            if (std::find(multiDelimiterQuoted.escapes.begin(), multiDelimiterQuoted.escapes.end(), ch) != multiDelimiterQuoted.escapes.end()) {
                multiDelimiterQuoted.used_escape = ch; // remember escape char
                CharT next;
                if (!is.get(next)) {
                    is.setstate(std::ios::failbit); // dangling escape
                    return is;
                }
                s.push_back(next);
            }
            // Handle closing delimiter
            else if (ch == multiDelimiterQuoted.used_delim) {
                return is; // success
            }
            // Normal character
            else {
                s.push_back(ch);
            }
        }

        // If we reach EOF before closing delimiter → fail
        is.setstate(std::ios::failbit);
        return is;
    }

private:
    string_type& s;
    std::vector<CharT> delimiters;
    std::vector<CharT> escapes;
    mutable CharT used_delim;   // which delimiter was actually used
    mutable CharT used_escape;  // which escape was actually used
};

#endif /* multiDelimiterQuoted_h */