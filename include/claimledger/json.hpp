#pragma once

#include <sstream>
#include <string>
#include <string_view>

namespace claimledger {

class JsonWriter {
public:
    explicit JsonWriter(int indent = 2);

    void begin_object();
    void end_object();
    void begin_array();
    void end_array();

    void key(std::string_view k);
    void value(std::nullptr_t);
    void value(bool v);
    void value(int v);
    void value(double v);
    void value(std::string_view v);
    void value(const std::string& v) { value(std::string_view(v)); }
    void value(const char* v) { value(std::string_view(v ? v : "")); }

    [[nodiscard]] std::string str() const { return out_.str(); }

private:
    void comma();
    void indent();
    void raw(std::string_view s);

    std::ostringstream out_;
    int indent_size_ = 2;
    int depth_ = 0;
    bool needs_comma_ = false;
    bool after_key_ = false;
};

std::string json_escape(std::string_view s);

}  // namespace claimledger
