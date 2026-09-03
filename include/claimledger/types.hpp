#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace claimledger {

inline constexpr std::string_view kVersion = "0.2.0";

using NodeId = std::int32_t;
using PaperId = std::string;

struct Paper {
    PaperId id;
    std::string title;
    int year = 0;
    std::string category;
    std::string field;
    std::string authors;
};

struct Category {
    std::string id;
    std::string name;
    std::string group;
};

struct Citation {
    PaperId citing;
    PaperId cited;
    int year = 0;
};

struct EigenPair {
    double value = 0.0;
    std::vector<double> vector;
};

}  // namespace claimledger
