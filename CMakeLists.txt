cmake_minimum_required(VERSION 3.16)
project(sharppy_cpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# GPU painting for the skew-T and hodograph (QOpenGLWidget). Turn off with -DSHARPPY_OPENGL=OFF.
option(SHARPPY_OPENGL "Draw the skew-T and hodograph with the GPU (QOpenGLWidget)" ON)

find_package(Qt6 COMPONENTS Widgets Core Gui Network QUIET)
if (NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Widgets Core Gui Network REQUIRED)
    set(QT_LIBS Qt5::Widgets Qt5::Core Qt5::Gui Qt5::Network)
else()
    set(QT_LIBS Qt6::Widgets Qt6::Core Qt6::Gui Qt6::Network)
    if (SHARPPY_OPENGL)
        find_package(Qt6 COMPONENTS OpenGLWidgets REQUIRED)    # QOpenGLWidget lives here in Qt 6
        list(APPEND QT_LIBS Qt6::OpenGLWidgets)
    endif()
endif()

include_directories(include)

file(GLOB SOURCES "src/*.cpp")
file(GLOB HEADERS "include/sharppy/*.hpp")

add_executable(sharppy_cpp ${SOURCES} ${HEADERS})
target_link_libraries(sharppy_cpp PRIVATE ${QT_LIBS})
if (SHARPPY_OPENGL)
    target_compile_definitions(sharppy_cpp PRIVATE SHARPPY_OPENGL)
endif()

# Datasource xml/csv files (and optional shapefiles) are read at run time from data/ next to the exe.
if(EXISTS "${CMAKE_SOURCE_DIR}/data")
    add_custom_command(TARGET sharppy_cpp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:sharppy_cpp>/data")
endif()
