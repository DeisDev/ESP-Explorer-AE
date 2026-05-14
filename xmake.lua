includes("lib/commonlibf4")

set_project("ESPExplorerAE")
set_version("1.5.1")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.release", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

add_requires("imgui v1.92.7", { configs = { dx11 = true, win32 = true } })
add_requires("simpleini v4.25")

target("ESPExplorerAE")
    add_rules("commonlibf4.plugin", {
        name = "ESPExplorerAE",
        author = "DeisDev",
        description = "In-game ESP/ESL/ESM Archive Explorer"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")

    add_packages("imgui", "simpleini")

    add_syslinks("comdlg32")

    add_installfiles("dist/fonts/*.ttf", { prefixdir = "Interface/ESPExplorerAE/fonts" })
    add_installfiles("dist/lang/*.ini", { prefixdir = "Interface/ESPExplorerAE/lang" })
    add_installfiles("dist/themes/*.ini", { prefixdir = "Interface/ESPExplorerAE/themes" })
