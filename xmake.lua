includes("lib/commonlibf4")

set_project("ESPExplorerAE")
set_version("2.0.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.release", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

add_requires("imgui v1.92.7", { configs = { dx11 = true, win32 = true } })
add_requires("simpleini v4.25")

target("ESPExplorerAE")
    set_warnings("allextra", "error")
    add_rules("commonlibf4.plugin", {
        name = "ESPExplorerAE",
        author = "DeisDev",
        description = "In-game ESP/ESL/ESM Archive Explorer",
        xse_minimum = "0.7.7"
    })

    -- Deploy explicitly with xmake install after validating the build.
    set_values("commonlib.plugin.install", false)

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
    -- Keep matching debug symbols for optimized builds.
    set_symbols("debug")

    add_packages("imgui", "simpleini")

    add_syslinks("comdlg32", "bcrypt")

    add_installfiles("dist/fonts/*.ttf", { prefixdir = "Interface/ESPExplorerAE/fonts" })
    add_installfiles("dist/fonts/manifest.json", "dist/fonts/README.md", { prefixdir = "Interface/ESPExplorerAE/fonts" })
    add_installfiles("dist/fonts/licenses/*.txt", { prefixdir = "Interface/ESPExplorerAE/fonts/licenses" })
    add_installfiles("dist/lang/*.ini", { prefixdir = "Interface/ESPExplorerAE/lang" })
    add_installfiles("dist/themes/*.ini", { prefixdir = "Interface/ESPExplorerAE/themes" })
    add_installfiles("dist/licenses/*.txt", "dist/licenses/manifest.json", { prefixdir = "Interface/ESPExplorerAE/licenses" })

target_end()

if os.isfile(".xmake-local.lua") then
    includes(".xmake-local.lua")
end
