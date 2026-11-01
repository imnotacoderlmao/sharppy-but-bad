add_rules("mode.debug", "mode.release")

-- `xmake f --embed_data=n` keeps the old behaviour: a data.dbpack file next to the executable.
option("embed_data")
    set_default(true)
    set_showmenu(true)
    set_description("Append the data archive to the executable so the program is self-contained")
option_end()

if is_plat("windows") then
    add_requires("mingw-w64@qt6", {configs = {widgets = true, network = true}})
else
    add_rules("qt.widgetapp")
end

-- Last build step (after linking; strip/UPX/signing must come BEFORE it, they drop the appended archive).
-- Re-running replaces an earlier embedding, so it is safe on every build.
local function pack_data(target)
    local src = path.join(os.projectdir(), "data")
    if not os.isdir(src) then return end
    local tool = path.join(os.projectdir(), "tools", "pack_db.py")
    if has_config("embed_data") then
        os.vexec("python \"%s\" embed \"%s\" \"%s\"", tool, src, target:targetfile())
    else
        os.vexec("python \"%s\" pack \"%s\" \"%s\" --if-changed", tool, src, path.join(target:targetdir(), "data.dbpack"))
    end
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

    after_build(pack_data)
    -- `xmake install`: the exe already carries data/ when embedded; otherwise ship the archive beside it
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "bin", path.filename(target:targetfile())))
        if not has_config("embed_data") then
            os.cp(path.join(target:targetdir(), "data.dbpack"), path.join(target:installdir(), "bin"))
        end
    end)
