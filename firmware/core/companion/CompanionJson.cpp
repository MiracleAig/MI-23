#include "core/companion/CompanionJson.h"

#include <cctype>
#include <limits>
#include <utility>

namespace Companion {

namespace {

constexpr int kMaxJsonDepth = 8;

bool isHexDigit(char ch) {
    return (ch >= '0' && ch <= '9') ||
        (ch >= 'a' && ch <= 'f') ||
        (ch >= 'A' && ch <= 'F');
}

int hexValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

class Parser {
public:
    Parser(const std::string& text, std::string* error)
        : m_text(text)
        , m_error(error)
        , m_pos(0) {}

    bool parse(JsonValue& out) {
        skipWhitespace();
        if (!parseValue(out, 0)) {
            return false;
        }
        skipWhitespace();
        if (m_pos != m_text.size()) {
            return fail("Unexpected trailing data");
        }
        return true;
    }

private:
    const std::string& m_text;
    std::string* m_error;
    std::size_t m_pos;

    bool fail(const char* message) {
        if (m_error) {
            *m_error = message ? message : "Invalid JSON";
        }
        return false;
    }

    void skipWhitespace() {
        while (m_pos < m_text.size() &&
               std::isspace(static_cast<unsigned char>(m_text[m_pos]))) {
            m_pos++;
        }
    }

    bool consume(char expected) {
        if (m_pos >= m_text.size() || m_text[m_pos] != expected) {
            return false;
        }
        m_pos++;
        return true;
    }

    bool parseValue(JsonValue& out, int depth) {
        if (depth > kMaxJsonDepth) {
            return fail("JSON nesting is too deep");
        }

        skipWhitespace();
        if (m_pos >= m_text.size()) {
            return fail("Unexpected end of JSON");
        }

        const char ch = m_text[m_pos];
        if (ch == '{') {
            return parseObject(out, depth + 1);
        }
        if (ch == '"') {
            std::string value;
            if (!parseString(value)) {
                return false;
            }
            out = JsonValue::string(std::move(value));
            return true;
        }
        if (ch == '-' || (ch >= '0' && ch <= '9')) {
            int64_t value = 0;
            if (!parseInteger(value)) {
                return false;
            }
            out = JsonValue::integer(value);
            return true;
        }
        if (m_text.compare(m_pos, 4, "true") == 0) {
            m_pos += 4;
            out = JsonValue::boolean(true);
            return true;
        }
        if (m_text.compare(m_pos, 5, "false") == 0) {
            m_pos += 5;
            out = JsonValue::boolean(false);
            return true;
        }
        if (m_text.compare(m_pos, 4, "null") == 0) {
            m_pos += 4;
            out = JsonValue();
            return true;
        }

        return fail("Unexpected JSON value");
    }

    bool parseObject(JsonValue& out, int depth) {
        if (!consume('{')) {
            return fail("Expected object");
        }

        std::map<std::string, JsonValue> object;
        skipWhitespace();
        if (consume('}')) {
            out = JsonValue::object(std::move(object));
            return true;
        }

        while (m_pos < m_text.size()) {
            std::string key;
            if (!parseString(key)) {
                return false;
            }

            skipWhitespace();
            if (!consume(':')) {
                return fail("Expected ':' after object key");
            }

            JsonValue value;
            if (!parseValue(value, depth + 1)) {
                return false;
            }
            object[key] = std::move(value);

            skipWhitespace();
            if (consume('}')) {
                out = JsonValue::object(std::move(object));
                return true;
            }
            if (!consume(',')) {
                return fail("Expected ',' or '}' in object");
            }
            skipWhitespace();
        }

        return fail("Unterminated object");
    }

    bool parseString(std::string& out) {
        if (!consume('"')) {
            return fail("Expected string");
        }

        out.clear();
        while (m_pos < m_text.size()) {
            const char ch = m_text[m_pos++];
            if (ch == '"') {
                return true;
            }
            if (static_cast<unsigned char>(ch) < 0x20u) {
                return fail("Control character in string");
            }
            if (ch != '\\') {
                out.push_back(ch);
                continue;
            }

            if (m_pos >= m_text.size()) {
                return fail("Unterminated escape sequence");
            }
            const char escaped = m_text[m_pos++];
            switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    out.push_back(escaped);
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u': {
                    if (m_pos + 4 > m_text.size()) {
                        return fail("Incomplete unicode escape");
                    }
                    int codepoint = 0;
                    for (int i = 0; i < 4; ++i) {
                        const char hex = m_text[m_pos + static_cast<std::size_t>(i)];
                        if (!isHexDigit(hex)) {
                            return fail("Invalid unicode escape");
                        }
                        codepoint = (codepoint << 4) | hexValue(hex);
                    }
                    m_pos += 4;
                    if (codepoint <= 0x7F) {
                        out.push_back(static_cast<char>(codepoint));
                    } else {
                        return fail("Only ASCII unicode escapes are supported");
                    }
                    break;
                }
                default:
                    return fail("Invalid escape sequence");
            }
        }

        return fail("Unterminated string");
    }

    bool parseInteger(int64_t& out) {
        bool negative = false;
        if (consume('-')) {
            negative = true;
        }

        if (m_pos >= m_text.size() || !std::isdigit(static_cast<unsigned char>(m_text[m_pos]))) {
            return fail("Expected integer");
        }

        if (m_text[m_pos] == '0') {
            m_pos++;
            if (m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos]))) {
                return fail("Leading zero in integer");
            }
            out = 0;
            return true;
        }

        uint64_t value = 0;
        const uint64_t positiveLimit = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
        const uint64_t negativeLimit = positiveLimit + 1u;
        const uint64_t limit = negative ? negativeLimit : positiveLimit;

        while (m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos]))) {
            const uint64_t digit = static_cast<uint64_t>(m_text[m_pos] - '0');
            if (value > (limit - digit) / 10u) {
                return fail("Integer overflow");
            }
            value = value * 10u + digit;
            m_pos++;
        }

        if (negative) {
            if (value == negativeLimit) {
                out = std::numeric_limits<int64_t>::min();
            } else {
                out = -static_cast<int64_t>(value);
            }
        } else {
            out = static_cast<int64_t>(value);
        }
        return true;
    }
};

} // namespace

JsonValue::JsonValue()
    : m_type(Type::Null)
    , m_integerValue(0)
    , m_boolValue(false) {}

JsonValue JsonValue::object(std::map<std::string, JsonValue> value) {
    JsonValue json;
    json.m_type = Type::Object;
    json.m_objectValue = std::move(value);
    return json;
}

JsonValue JsonValue::string(std::string value) {
    JsonValue json;
    json.m_type = Type::String;
    json.m_stringValue = std::move(value);
    return json;
}

JsonValue JsonValue::integer(int64_t value) {
    JsonValue json;
    json.m_type = Type::Integer;
    json.m_integerValue = value;
    return json;
}

JsonValue JsonValue::boolean(bool value) {
    JsonValue json;
    json.m_type = Type::Boolean;
    json.m_boolValue = value;
    return json;
}

JsonValue::Type JsonValue::type() const {
    return m_type;
}

bool JsonValue::isObject() const {
    return m_type == Type::Object;
}

bool JsonValue::isString() const {
    return m_type == Type::String;
}

bool JsonValue::isInteger() const {
    return m_type == Type::Integer;
}

bool JsonValue::isBoolean() const {
    return m_type == Type::Boolean;
}

const std::map<std::string, JsonValue>& JsonValue::objectItems() const {
    return m_objectValue;
}

const std::string& JsonValue::stringValue() const {
    return m_stringValue;
}

int64_t JsonValue::integerValue() const {
    return m_integerValue;
}

bool JsonValue::boolValue() const {
    return m_boolValue;
}

const JsonValue* JsonValue::get(const char* key) const {
    if (!key || !isObject()) {
        return nullptr;
    }
    const auto found = m_objectValue.find(key);
    return found == m_objectValue.end() ? nullptr : &found->second;
}

bool parseJson(const std::string& text, JsonValue& out, std::string* error) {
    Parser parser(text, error);
    return parser.parse(out);
}

} // namespace Companion
