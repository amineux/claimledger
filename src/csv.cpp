#include "claimledger/csv.hpp"

#include <fstream>
#include <stdexcept>

namespace claimledger {

std::vector<std::string> parse_csv_line(std::string_view line) {
    std::vector<std::string> fields;
    std::string cur;
    bool in_quotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (in_quotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    cur.push_back('"');
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                cur.push_back(c);
            }
        } else {
            if (c == '"') {
                in_quotes = true;
            } else if (c == ',') {
                fields.push_back(std::move(cur));
                cur.clear();
            } else if (c != '\r') {
                cur.push_back(c);
            }
        }
    }
    fields.push_back(std::move(cur));
    return fields;
}

std::string csv_escape(std::string_view field) {
    bool need = false;
    for (char c : field) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            need = true;
            break;
        }
    }
    if (!need) {
        return std::string(field);
    }
    std::string out;
    out.push_back('"');
    for (char c : field) {
        if (c == '"') {
            out += "\"\"";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}

int CsvTable::column(std::string_view name) const {
    for (int i = 0; i < static_cast<int>(header.size()); ++i) {
        if (header[static_cast<std::size_t>(i)] == name) {
            return i;
        }
    }
    return -1;
}

std::string_view CsvTable::get(const std::vector<std::string>& row, std::string_view name) const {
    const int i = column(name);
    if (i < 0 || i >= static_cast<int>(row.size())) {
        return {};
    }
    return row[static_cast<std::size_t>(i)];
}

CsvTable read_csv(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open CSV: " + path);
    }
    CsvTable t;
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        auto fields = parse_csv_line(line);
        if (first) {
            t.header = std::move(fields);
            first = false;
        } else {
            t.rows.push_back(std::move(fields));
        }
    }
    if (first) {
        throw std::runtime_error("empty CSV: " + path);
    }
    return t;
}

}  // namespace claimledger
