set_project("XnnTransfer")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

set_languages("c++20")
add_includedirs("src", "$(builddir)")
add_requires("fmt", "spdlog", "nlohmann_json", "asio", "gtest", "protobuf-cpp", "openssl")

if is_plat("macosx") then
    set_toolchains("gcc", "clang")
    add_cxxflags("-std=c++20", "-fconcepts")
end

target("XnnTransfer")
    set_kind("binary")
    add_rules("protobuf.cpp")
    add_files("src/**.cc")
    add_files("proto/*.proto")
    add_packages("fmt", "spdlog", "nlohmann_json", "asio", "protobuf-cpp", "openssl")

    if is_mode("debug") then
        set_symbols("debug")
        set_optimize("none")
    end
    
    if is_plat("windows") then
        add_syslinks("ws2_32", "iphlpapi", "shell32")
    end

target("tests")
    set_kind("binary")
    set_default(false)
    add_rules("protobuf.cpp")

    add_packages("gtest", "fmt", "spdlog", "nlohmann_json", "asio", "protobuf-cpp", "openssl")
    add_tests("default")

    add_files("src/**.cc")
    add_files("proto/*.proto")
    add_files("tests/**.cc")
    remove_files("src/cli/main.cc")
    add_links("gtest_main")

    if is_plat("windows") then
        add_syslinks("ws2_32", "iphlpapi", "shell32")
        add_ldflags("/WHOLEARCHIVE:gtest_main.lib", {force = true})
    end