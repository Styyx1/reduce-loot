if is_plat("windows") then
    add_cxxflags("cl::/std:c++23preview", {force = true})
else
    set_languages("c++23")
end
includes("lib/commonlibsse", "extern/styyx-utils")
local MOD_NAME = "styyx-reduce-loot"
local MOD_VERSION = "1.0.0"
local MOD_DESC = "reduces loot gained"
set_project(MOD_NAME)
set_version(MOD_VERSION)
set_license("GPL-3.0")
set_warnings("allextra")
set_config("commonlib_toml", true)
set_config("use-fui", true)
add_rules("mode.debug", "mode.releasedbg")
set_defaultmode("releasedbg")
target(MOD_NAME)
    add_deps("styyx-util")
    add_rules("commonlibsse.plugin", {
        name = MOD_NAME,
        author = "styyx",
        description = MOD_DESC
    })
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
    add_installfiles("res/(**)")