add_rules("mode.debug", "mode.release")

if is_plat("windows") then
    add_requires("mingw-w64@qt6", {configs = {widgets = true, network = true}})
    else
        add_rules("qt.widgetapp")
end

local function pack_n_copy_data (target)
    import("core.project.depend")

    local src = path.join(os.projectdir(), "data")
    local dst = path.join(target:targetdir(), "data.dbpack")
    local dep_file = path.join(target:targetdir(), "data.dbpack.d")

    depend.on_changed(function ()
        os.exec("python tools/pack_db.py pack %s %s", src, dst)
    end, {
        files = path.join(src, "**"),
                dependfile = dep_file
    })
end

target("sharppy_cpp")
    set_kind("binary")
    set_languages("c++17")

    add_includedirs("include")
    add_files("src/*.cpp")
    add_files("include/sharppy/*.hpp")

    if is_plat("windows") then
        add_packages("mingw-w64@qt6")
    else
        add_frameworks("QtWidgets", "QtGui", "QtCore", "QtNetwork")
    end

    after_build(pack_n_copy_data)
    -- `xmake install`: ship data/ with the binary
    add_installfiles("data/(**)", {prefixdir = "bin/data"})
