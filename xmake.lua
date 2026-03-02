includes("lib/commonlibf4")

set_project("ESPExplorerAE")
set_version("0.1.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

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

    add_files(
        "lib/imgui/imgui.cpp",
        "lib/imgui/imgui_demo.cpp",
        "lib/imgui/imgui_draw.cpp",
        "lib/imgui/imgui_tables.cpp",
        "lib/imgui/imgui_widgets.cpp",
        "lib/imgui/backends/imgui_impl_dx11.cpp",
        "lib/imgui/backends/imgui_impl_win32.cpp"
    )
    add_includedirs("lib/imgui", "lib/imgui/backends")

    add_includedirs("lib/simpleini")

    add_syslinks("d3d11", "dxgi", "d3dcompiler")
