add_rules("mode.debug", "mode.release")

if is_plat("windows") then
    add_requires("mingw-w64@qt6", {configs = {widgets = true, network = true}})
else
    add_rules("qt.widgetapp")
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

    after_build(function (target)
        -- datasource xml/csv (and shapefiles) are read at run time from data/ next to the exe
        if os.isdir("data") then
            os.cp("data/*", path.join(target:targetdir(), "data"))
        end
    end)
