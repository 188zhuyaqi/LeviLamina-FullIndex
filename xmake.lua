add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

option("target_type")
    set_default("server")
    set_showmenu(true)
    set_values("server", "client")
option_end()

-- 针对当前生产服固定 LeviLamina 版本，避免 CI 随最新版本漂移。
-- 后续升级服务器时，在单独分支验证后再更新。
add_requires("levilamina 26.10.11", {configs = {target_type = get_config("target_type")}})
add_requires("levibuildscript")
add_requires("nlohmann_json")
add_requires("ixwebsocket")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("FullIndex")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")

    if is_plat("windows") then
        add_defines("NOMINMAX", "UNICODE")
        set_exceptions("none")
        add_cxflags(
            "/EHa", "/utf-8", "/W4",
            "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204"
        )
        add_cxflags(
            "/EHs",
            "-Wno-microsoft-cast",
            "-Wno-invalid-offsetof",
            "-Wno-c++2b-extensions",
            "-Wno-microsoft-include",
            "-Wno-overloaded-virtual",
            "-Wno-ignored-qualifiers",
            "-Wno-missing-field-initializers",
            "-Wno-potentially-evaluated-expression",
            "-Wno-pragma-system-header-outside-header",
            {tools = {"clang_cl"}}
        )
        set_toolchains("clang-cl")
    end

    add_packages("levilamina", "nlohmann_json", "ixwebsocket")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")

    add_headerfiles("plugin/src/**.h")
    add_files("plugin/src/**.cpp")
    add_includedirs("plugin/src")
