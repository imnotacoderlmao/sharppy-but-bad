add_rules("mode.debug", "mode.release")

if is_plat("windows") then
    add_requires("mingw-w64@qt6", {configs = {widgets = true, network = true}})
else
    add_rules("qt.widgetapp")
end

-- Copies data/ (sars/sars.sarspack, datasources, shapefiles, sharptab) next to the exe.
-- The program looks for data/... beside the executable and in its parent folders.
local function sync_data(target)
    local src = path.join(os.projectdir(), "data")
    if not os.isdir(src) then return end
    local dst = path.join(target:targetdir(), "data")
    os.mkdir(dst)
    for _, entry in ipairs(os.filedirs(path.join(src, "*"))) do
        local name = path.filename(entry)
        if os.isdir(entry) then
            os.mkdir(path.join(dst, name))
            os.cp(path.join(entry, "*"), path.join(dst, name))
        else
            os.cp(entry, dst)
        end
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

    after_build(sync_data)      -- every build
    before_run(sync_data)       -- `xmake run` after only data/ changed (e.g. a repacked sars.sarspack)

    -- `xmake install`: ship data/ with the binary
    add_installfiles("data/(**)", {prefixdir = "bin/data"})
