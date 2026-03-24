#include <CLI/CLI.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

TEST_CASE("CLI requires --input", "[cli]") {
    CLI::App app{"t"};
    std::string input;
    app.add_option("-i,--input", input)->required();
    std::vector<char *> argv = {const_cast<char *>("t"), const_cast<char *>("-o"),
                                const_cast<char *>("out.jpg")};
    int ac = static_cast<int>(argv.size());
    REQUIRE_THROWS_AS(app.parse(ac, argv.data()), CLI::ParseError);
}

TEST_CASE("CLI accepts filters and quality", "[cli]") {
    CLI::App app{"t"};
    std::string input;
    std::vector<std::string> filters;
    std::optional<int> q;
    app.add_option("-i,--input", input)->required();
    app.add_option("-f,--filter", filters)->expected(-1);
    app.add_option("-q,--quality", q)->check(CLI::Range(1, 100));
    std::vector<char *> argv = {const_cast<char *>("t"), const_cast<char *>("-i"),
                                const_cast<char *>("a.png"), const_cast<char *>("-f"),
                                const_cast<char *>("blur"), const_cast<char *>("-q"),
                                const_cast<char *>("40")};
    int ac = static_cast<int>(argv.size());
    REQUIRE_NOTHROW(app.parse(ac, argv.data()));
    REQUIRE(input == "a.png");
    REQUIRE(filters.size() == 1);
    REQUIRE(filters[0] == "blur");
    REQUIRE(q.has_value());
    REQUIRE(*q == 40);
}
