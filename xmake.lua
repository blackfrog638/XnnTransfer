add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

set_languages("c++20")
add_requires("fmt", "spdlog", "nlohmann_json", "asio")

target("XnnTransfer")
    set_kind("binary")
    add_files("src/cli/main.cpp")
    add_includedirs("src")
    add_packages("fmt", "spdlog", "nlohmann_json", "asio")

    if is_mode("debug") then
        set_symbols("debug")
        set_optimize("none")
    end
    
    if is_plat("windows") then
        add_syslinks("ws2_32", "iphlpapi")
    end