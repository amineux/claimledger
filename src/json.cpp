#include "claimledger/json.hpp"

#include <cmath>
#include <cstdio>
#include <iomanip>

namespace claimledger {

std::string json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

JsonWriter::JsonWriter(int indent) : indent_size_(indent) {}

void JsonWriter::raw(std::string_view s) { out_ << s; }

void JsonWriter::indent() {
    if (indent_size_ <= 0) {
        return;
    }
    out_ << '\n';
    for (int i = 0; i < depth_ * indent_size_; ++i) {
        out_ << ' ';
    }
}

void JsonWriter::comma() {
    if (after_key_) {
        return;
    }
    if (needs_comma_) {
        out_ << ',';
    }
    needs_comma_ = false;
}

void JsonWriter::begin_object() {
    comma();
    if (depth_ > 0 && !after_key_ && indent_size_ > 0) {
        indent();
    }
    after_key_ = false;
    out_ << '{';
    ++depth_;
    needs_comma_ = false;
}

void JsonWriter::end_object() {
    --depth_;
    if (needs_comma_ && indent_size_ > 0) {
        indent();
    }
    out_ << '}';
    needs_comma_ = true;
    after_key_ = false;
}

void JsonWriter::begin_array() {
    comma();
    if (depth_ > 0 && !after_key_ && indent_size_ > 0) {
        indent();
    }
    after_key_ = false;
    out_ << '[';
    ++depth_;
    needs_comma_ = false;
}

void JsonWriter::end_array() {
    --depth_;
    if (needs_comma_ && indent_size_ > 0) {
        indent();
    }
    out_ << ']';
    needs_comma_ = true;
    after_key_ = false;
}

void JsonWriter::key(std::string_view k) {
    comma();
    indent();
    out_ << '"' << json_escape(k) << "\": ";
    after_key_ = true;
    needs_comma_ = false;
}

void JsonWriter::value(std::nullptr_t) {
    comma();
    if (depth_ > 0 && !after_key_ && indent_size_ > 0) {
        indent();
    }
    after_key_ = false;
    out_ << "null";
    needs_comma_ = true;
}

void JsonWriter::value(bool v) {
    comma();
    if (depth_ > 0 && !after_key_ && indent_size_ > 0) {
        indent();
    }
    after_key_ = false;
    out_ << (v ? "true" : "false");
    needs_comma_ = true;
}

void JsonWriter::value(int v) {
    comma();
    if (depth_ > 0 && !after_key_ && indent_size_ > 0) {
        indent();
    }
    after_key_ = false;
    out_ << v;
    needs_comma_ = true;
}

void JsonWriter::value(double v) {
    comma();
    if (depth_ > 0 && !after_key_ && indent_size_ > 0) {
        indent();
    }
    after_key_ = false;
    if (!std::isfinite(v)) {
        out_ << "null";
    } else {
        out_ << std::setprecision(12) << v;
    }
    needs_comma_ = true;
}

void JsonWriter::value(std::string_view v) {
    comma();
    if (depth_ > 0 && !after_key_ && indent_size_ > 0) {
        indent();
    }
    after_key_ = false;
    out_ << '"' << json_escape(v) << '"';
    needs_comma_ = true;
}

}  // namespace claimledger
