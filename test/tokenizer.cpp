#include <doctest.h>
#include <string_view>
#include <iostream>
#include <ash/tokenizer.hpp>

TEST_CASE("line_matcher")
{
    constexpr std::string_view raw = R"(# Some comment
set x 42; foo 12; line breaker \

read-json '
{
    "foo" : "bar"
} '
foo bar; asd ;)";

    auto [line, rest] = ash::pick_line(raw);
    CHECK(line == R"(# Some comment
set x 42)");

    tie(line, rest)  = ash::pick_line(rest);
    CHECK(line == "foo 12");

    tie(line, rest)  = ash::pick_line(rest);
    CHECK(line == R"(line breaker \
)");


    tie(line, rest)  = ash::pick_line(rest);
    CHECK(line == R"(read-json '
{
    "foo" : "bar"
} ')");


    tie(line, rest)  = ash::pick_line(rest);
    CHECK(line == R"(foo bar)");

    tie(line, rest)  = ash::pick_line(rest);
    CHECK(line == R"(asd )");
    CHECK(rest.empty());
}

TEST_CASE("tokenizer")
{
    auto [tks, rest] = ash::tokenize(R"(# Some comment
set x 42)");

    CHECK(tks == std::vector<std::string_view>{"set", "x", "42"});
    CHECK(rest == "");

    tie(tks, rest) = ash::tokenize("foo 12");
    CHECK(tks == std::vector<std::string_view>{"foo", "12"});
    CHECK(rest == "");

    tie(tks, rest) = ash::tokenize(R"(line breaker \
broken)");
    CHECK(tks == std::vector<std::string_view>{"line", "breaker", "broken"});
    CHECK(rest == "");

    tie(tks, rest) = ash::tokenize(R"(read-json '
{
    "foo" : "bar"
} ')");
    REQUIRE(tks.size() == 2);
    CHECK(tks[0] == "read-json");
    CHECK(tks[1] == R"(
{
    "foo" : "bar"
}   )");

    CHECK(rest == "");

    tie(tks, rest) = ash::tokenize(R"(foo bar)");
    CHECK(tks == std::vector<std::string_view>{"foo", "bar"});
    CHECK(rest == "");


    tie(tks, rest) = ash::tokenize(R"(asd )");
    CHECK(tks == std::vector<std::string_view>{"asd"});
    CHECK(rest == "");

}
