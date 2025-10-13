set_project("XnnTransfer")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

set_languages("c++20")
add_includedirs("src")
add_requires("fmt", "spdlog", "nlohmann_json", "asio", "gtest")

-- Force using modern compilers on macOS
if is_plat("macosx") then
    -- Try to use Homebrew's GCC or LLVM instead of Apple Clang
    set_toolchains("gcc", "clang")
    -- Enable full C++20 support
    add_cxxflags("-std=c++20", "-fconcepts")
end

target("XnnTransfer")
    set_kind("binary")
    add_files("src/**.cc")
    add_packages("fmt", "spdlog", "nlohmann_json", "asio")

    if is_mode("debug") then
        set_symbols("debug")
        set_optimize("none")
    end
    
    if is_plat("windows") then
        add_syslinks("ws2_32", "iphlpapi")
    end

target("tests")
    set_kind("binary")
    set_default(false)

    add_packages("gtest", "fmt", "spdlog", "nlohmann_json", "asio")
    add_tests("default")

    add_files("src/**.cc")
    add_files("tests/**.cc")
    remove_files("src/cli/main.cc")
    add_links("gtest_main")

    if is_plat("windows") then
        add_syslinks("ws2_32", "iphlpapi")
        add_ldflags("/WHOLEARCHIVE:gtest_main.lib", {force = true})
    end