# ============================================================================
# ФАЙЛ CMAKELISTS.TXT - СЦЕНАРИЙ СБОРКИ ПРОЕКТА LAB_2
# ============================================================================

cmake_minimum_required(VERSION 3.16)

project(ModelingMethodsLab2 VERSION 0.1.0 LANGUAGES CXX)

function(_append_existing_prefixes out_var)
    set(prefixes ${${out_var}})
    foreach(path IN LISTS ARGN)
        if(path AND EXISTS "${path}")
            list(APPEND prefixes "${path}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES prefixes)
    set(${out_var} "${prefixes}" PARENT_SCOPE)
endfunction()

if(APPLE)
    set(_qt_candidate_prefixes "")

    if(NOT Qt6_DIR)
        if(EXISTS "/opt/homebrew/opt/qt/lib/cmake/Qt6")
            set(Qt6_DIR "/opt/homebrew/opt/qt/lib/cmake/Qt6")
        elseif(EXISTS "/usr/local/opt/qt/lib/cmake/Qt6")
            set(Qt6_DIR "/usr/local/opt/qt/lib/cmake/Qt6")
        endif()
    endif()

    if(DEFINED ENV{Qt6_DIR} AND EXISTS "$ENV{Qt6_DIR}")
        get_filename_component(_qt_prefix "$ENV{Qt6_DIR}/../.." ABSOLUTE)
        list(APPEND _qt_candidate_prefixes "${_qt_prefix}")
    endif()

    file(GLOB _qt_installer_prefixes LIST_DIRECTORIES true
        "/Applications/Qt/*/macos"
        "$ENV{HOME}/Qt/*/macos"
    )

    _append_existing_prefixes(
        _qt_candidate_prefixes
        ${_qt_installer_prefixes}
        "/opt/homebrew/opt/qt"
        "/opt/homebrew/opt/qt6"
        "/opt/homebrew/opt/qtbase"
        "/opt/homebrew/opt/qtsvg"
        "/opt/homebrew/opt/qtcharts"
        "/usr/local/opt/qt"
        "/usr/local/opt/qt6"
        "/usr/local/opt/qtbase"
        "/usr/local/opt/qtsvg"
        "/usr/local/opt/qtcharts"
    )

    if(_qt_candidate_prefixes)
        list(PREPEND CMAKE_PREFIX_PATH ${_qt_candidate_prefixes})
        list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
    endif()

    set(_qt_additional_package_prefixes "")
    _append_existing_prefixes(
        _qt_additional_package_prefixes
        "/opt/homebrew/opt/qt"
        "/opt/homebrew/opt/qtsvg"
        "/opt/homebrew/opt/qtcharts"
        "/usr/local/opt/qt"
        "/usr/local/opt/qtsvg"
        "/usr/local/opt/qtcharts"
    )

    if(_qt_additional_package_prefixes)
        list(JOIN _qt_additional_package_prefixes ";" QT_ADDITIONAL_PACKAGES_PREFIX_PATH)
        set(QT_ADDITIONAL_PACKAGES_PREFIX_PATH
            "${QT_ADDITIONAL_PACKAGES_PREFIX_PATH}"
            CACHE STRING "Additional Qt package roots" FORCE)
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts Concurrent)

add_executable(
    ModelingMethodsLab2
    MACOSX_BUNDLE
    main.cpp
    include/InteractiveChartView.h
    include/Lab2Variant.h
    include/MathWidgets.h
    include/MainWindow.h
    include/PlotChartWidget.h
    include/PlotChartUtils.h
    include/PresentationBuilders.h
    include/SamplingEngine.h
    include/SamplingTypes.h
    include/TableExport.h
    include/TableModels.h
    include/UiFormatting.h
    src/InteractiveChartView.cpp
    src/MathWidgets.cpp
    src/MainWindow.cpp
    src/PlotChartWidget.cpp
    src/PlotChartUtils.cpp
    src/PresentationBuilders.cpp
    src/SamplingEngine.cpp
    src/TableExport.cpp
    src/TableModels.cpp
    src/UiFormatting.cpp
)

target_include_directories(ModelingMethodsLab2 PRIVATE include)

target_compile_features(ModelingMethodsLab2 PRIVATE cxx_std_17)

target_compile_definitions(
    ModelingMethodsLab2
    PRIVATE
        MODELING_METHODS_LAB2_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

set_target_properties(
    ModelingMethodsLab2
    PROPERTIES
        AUTOMOC ON
        AUTORCC ON
        CXX_EXTENSIONS OFF
        MACOSX_BUNDLE_BUNDLE_NAME "ModelingMethodsLab2"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.modelingmethods.lab2"
)

target_link_libraries(ModelingMethodsLab2 PRIVATE Qt6::Widgets Qt6::Charts Qt6::Concurrent)
