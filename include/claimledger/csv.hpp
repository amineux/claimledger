#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace claimledger {

std::vector<std::string> parse_csv_line(std::string_view line);
std::string csv_escape(std::string_view field);

struct CsvTable {
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> rows;

    [[nodiscard]] int column(std::string_view name) const;
    [[nodiscard]] std::string_view get(const std::vector<std::string>& row, std::string_view name) const;
};

CsvTable read_csv(const std::string& path);

}  // namespace claimledger
