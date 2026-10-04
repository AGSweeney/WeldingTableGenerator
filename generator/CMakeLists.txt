# Copyright (c) 2026 Adam G. Sweeney <AGSweeney@gmail.com>
# SPDX-License-Identifier: MIT
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.21)
project(WeldingTableGenerator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)

if(NOT VTK_DIR AND EXISTS "C:/Users/Adam/vcpkg/installed/x64-windows/share/vtk/vtk-config.cmake")
    set(VTK_DIR "C:/Users/Adam/vcpkg/installed/x64-windows/share/vtk" CACHE PATH "VTK config directory")
endif()
find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    RenderingCore
    RenderingOpenGL2
    RenderingAnnotation
    RenderingFreeType
    InteractionStyle
    InteractionWidgets
    FiltersSources
    FiltersCore
    FiltersModeling
    GUISupportQt
)

qt_standard_project_setup()

qt_add_executable(WeldingTableGenerator WIN32
    src/main.cpp
    src/AppVersion.h
    src/DxfAudit.cpp
    src/DxfAudit.h
    src/AppStyle.cpp
    src/AppStyle.h
    src/TableModel.cpp
    src/TableModel.h
    src/DxfWriter.cpp
    src/DxfWriter.h
    src/PaintTable.cpp
    src/PaintTable.h
    src/PackageWriter.cpp
    src/PackageWriter.h
    src/PreviewWidget.cpp
    src/PreviewWidget.h
    src/VtkTableView.cpp
    src/VtkTableView.h
    src/MainWindow.cpp
    src/MainWindow.h
)

set(ANEST_ROOT "D:/ANest" CACHE PATH "ANest tree with the built libdxfrw")
target_include_directories(WeldingTableGenerator PRIVATE "${ANEST_ROOT}/third_party/libdxfrw/src")

target_link_libraries(WeldingTableGenerator PRIVATE
    Qt6::Widgets
    Qt6::OpenGLWidgets
    "${ANEST_ROOT}/build/libdxfrw/Release/dxfrw.lib"
    VTK::CommonCore
    VTK::CommonDataModel
    VTK::RenderingCore
    VTK::RenderingOpenGL2
    VTK::RenderingAnnotation
    VTK::RenderingFreeType
    VTK::InteractionStyle
    VTK::InteractionWidgets
    VTK::FiltersSources
    VTK::FiltersCore
    VTK::FiltersModeling
    VTK::GUISupportQt
)

vtk_module_autoinit(
    TARGETS WeldingTableGenerator
    MODULES VTK::RenderingOpenGL2 VTK::InteractionStyle VTK::RenderingFreeType VTK::RenderingAnnotation
)

qt_add_resources(WeldingTableGenerator app_icons
    PREFIX "/icons/ui"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/ui"
    FILES
        resources/icons/ui/chevron-up.svg
        resources/icons/ui/chevron-down.svg
)

qt_add_resources(WeldingTableGenerator app_brand
    PREFIX "/icons/app"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app"
    FILES
        resources/icons/app/welding-table.svg
)

if(WIN32)
    target_sources(WeldingTableGenerator PRIVATE resources/icons/app/app.rc)
endif()
set_target_properties(WeldingTableGenerator PROPERTIES OUTPUT_NAME "Welding Table Generator")

if(WIN32)
    if(DEFINED VTK_DIR)
        get_filename_component(_vtk_bin "${VTK_DIR}/../../bin" ABSOLUTE)
        if(EXISTS "${_vtk_bin}/vtkCommonCore-9.3.dll")
            add_custom_command(TARGET WeldingTableGenerator POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:WeldingTableGenerator>
                    $<TARGET_FILE_DIR:WeldingTableGenerator>
                COMMAND ${CMAKE_COMMAND}
                    -DDEST_DIR=$<TARGET_FILE_DIR:WeldingTableGenerator>
                    -DTRIPLET_BIN=${_vtk_bin}
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyVtkRuntime.cmake"
                COMMAND_EXPAND_LISTS
                COMMENT "Copying VTK runtime DLLs"
            )
        endif()
    endif()
    get_target_property(_qt_qmake Qt6::qmake IMPORTED_LOCATION)
    if(_qt_qmake)
        get_filename_component(_qt_bin_dir "${_qt_qmake}" DIRECTORY)
        set(_windeployqt "${_qt_bin_dir}/windeployqt.exe")
        if(EXISTS "${_windeployqt}")
            add_custom_command(TARGET WeldingTableGenerator POST_BUILD
                COMMAND "${_windeployqt}"
                    --force
                    --no-compiler-runtime
                    --no-translations
                    "$<TARGET_FILE:WeldingTableGenerator>"
                COMMENT "Deploying Qt runtime (windeployqt)"
            )
        endif()
    endif()
endif()
