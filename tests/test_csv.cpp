#include "claimledger/csv.hpp"
#include "claimledger/json.hpp"

#include <gtest/gtest.h>

using namespace claimledger;

TEST(Csv, QuotedCommas) {
    auto f = parse_csv_line("synth-0001,\"Spectral methods for cats, dogs\",2018,cs.LG");
    ASSERT_EQ(f.size(), 4u);
    EXPECT_EQ(f[0], "synth-0001");
    EXPECT_EQ(f[1], "Spectral methods for cats, dogs");
    EXPECT_EQ(f[2], "2018");
}

TEST(Csv, EscapedQuotes) {
    auto f = parse_csv_line("a,\"she said \"\"hello\"\"\",b");
    ASSERT_EQ(f.size(), 3u);
    EXPECT_EQ(f[1], "she said \"hello\"");
}

TEST(Csv, RoundTripEscape) {
    EXPECT_EQ(csv_escape("plain"), "plain");
    EXPECT_EQ(csv_escape("a,b"), "\"a,b\"");
    EXPECT_EQ(csv_escape("say \"hi\""), "\"say \"\"hi\"\"\"");
}

TEST(Json, Escapes) {
    EXPECT_EQ(json_escape("a\"b"), "a\\\"b");
    EXPECT_EQ(json_escape("line\nbreak"), "line\\nbreak");
}

TEST(Json, WriterObject) {
    JsonWriter j(0);
    j.begin_object();
    j.key("n");
    j.value(3);
    j.key("ok");
    j.value(true);
    j.key("name");
    j.value("synth-0001");
    j.end_object();
    const auto s = j.str();
    EXPECT_NE(s.find("\"n\": 3"), std::string::npos);
    EXPECT_NE(s.find("\"ok\": true"), std::string::npos);
    EXPECT_NE(s.find("synth-0001"), std::string::npos);
}
