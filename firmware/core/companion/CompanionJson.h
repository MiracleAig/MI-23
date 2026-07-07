#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace Companion {

class JsonValue {
public:
    enum class Type {
        Null,
        Object,
        String,
        Integer,
        Boolean,
    };

    JsonValue();

    static JsonValue object(std::map<std::string, JsonValue> value);
    static JsonValue string(std::string value);
    static JsonValue integer(int64_t value);
    static JsonValue boolean(bool value);

    Type type() const;
    bool isObject() const;
    bool isString() const;
    bool isInteger() const;
    bool isBoolean() const;

    const std::map<std::string, JsonValue>& objectItems() const;
    const std::string& stringValue() const;
    int64_t integerValue() const;
    bool boolValue() const;

    const JsonValue* get(const char* key) const;

private:
    Type m_type;
    std::map<std::string, JsonValue> m_objectValue;
    std::string m_stringValue;
    int64_t m_integerValue;
    bool m_boolValue;
};

bool parseJson(const std::string& text, JsonValue& out, std::string* error = nullptr);

} // namespace Companion
