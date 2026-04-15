# ============================================================================
# ФАЙЛ CMAKELISTS.TXT - СЦЕНАРИЙ СБОРКИ ПРОЕКТА LAB_2
# ============================================================================
# Назначение файла:
# 1) описать правила сборки приложения на C++/Qt;
# 2) подготовить поиск Qt на macOS в типовых местах установки;
# 3) подключить нужные Qt-модули;
# 4) объявить исполняемый файл приложения и все исходники проекта;
# 5) настроить свойства цели Qt/CMake;
# 6) связать приложение с требуемыми библиотеками Qt.
#
# Иными словами, именно этот файл превращает набор исходников проекта
# в реально собираемое desktop-приложение.
# ============================================================================

# Команда cmake_minimum_required задает минимальную версию CMake,
# ниже которой данный файл конфигурации запускать нельзя.
cmake_minimum_required(VERSION 3.16)

# Команда project объявляет сам проект CMake.
# Здесь задается:
# - имя проекта;
# - версия проекта;
# - список используемых языков.
# LANGUAGES CXX означает, что проект собирается как C++-проект.
project(ModelingMethodsLab2 VERSION 0.1.0 LANGUAGES CXX)

# ----------------------------------------------------------------------------
# ФУНКЦИЯ _append_existing_prefixes
# ----------------------------------------------------------------------------
# Назначение функции:
# Добавить в список путей только те каталоги, которые реально существуют
# на текущей машине.
#
# Зачем это нужно:
# При поиске Qt на macOS мы хотим проверить несколько типовых мест установки,
# но не хотим засорять CMAKE_PREFIX_PATH несуществующими каталогами.
#
# Принимает:
# - out_var : имя переменной, в которую нужно вернуть итоговый список;
# - ARGN    : все остальные переданные пути-кандидаты.
# ----------------------------------------------------------------------------
function(_append_existing_prefixes out_var)
    # Читаем текущее содержимое выходной переменной во временный список.
    # Конструкция ${${out_var}} означает:
    # 1) сначала взять имя переменной из out_var;
    # 2) затем прочитать значение переменной с таким именем.
    set(prefixes ${${out_var}})

    # Перебираем все дополнительные аргументы функции как список путей.
    foreach(path IN LISTS ARGN)
        # Добавляем путь только если:
        # 1) строка не пустая;
        # 2) такой путь реально существует в файловой системе.
        if(path AND EXISTS "${path}")
            list(APPEND prefixes "${path}")
        endif()
    endforeach()

    # Удаляем дубликаты, чтобы один и тот же путь не попадал в список дважды.
    list(REMOVE_DUPLICATES prefixes)

    # Возвращаем результат во внешнюю область видимости.
    # PARENT_SCOPE означает, что обновленное значение получит вызывающий код,
    # а не только тело самой функции.
    set(${out_var} "${prefixes}" PARENT_SCOPE)
endfunction()

# ----------------------------------------------------------------------------
# ПОДГОТОВКА ПУТЕЙ ПОИСКА QT ДЛЯ macOS
# ----------------------------------------------------------------------------
# На macOS Qt может быть установлен несколькими способами:
# - через официальный Qt Installer;
# - через Homebrew на Apple Silicon;
# - через Homebrew на Intel.
#
# Ниже собираем разумный список возможных корней установки Qt, чтобы
# find_package(Qt6 ...) мог сработать без ручной подстройки под каждую машину.
# ----------------------------------------------------------------------------
if(APPLE)
    # Создаем пустой список возможных корней установки Qt.
    set(_qt_candidate_prefixes "")

    # Если Qt6_DIR еще не задан явно, пробуем определить его по типовым путям
    # Homebrew, где лежат CMake-конфиги Qt6.
    if(NOT Qt6_DIR)
        # Типовой путь Homebrew на Apple Silicon.
        if(EXISTS "/opt/homebrew/opt/qt/lib/cmake/Qt6")
            set(Qt6_DIR "/opt/homebrew/opt/qt/lib/cmake/Qt6")
        # Типовой путь Homebrew на Intel.
        elseif(EXISTS "/usr/local/opt/qt/lib/cmake/Qt6")
            set(Qt6_DIR "/usr/local/opt/qt/lib/cmake/Qt6")
        endif()
    endif()

    # Поддерживаем сценарий, когда Qt6_DIR передан через переменную окружения.
    if(DEFINED ENV{Qt6_DIR} AND EXISTS "$ENV{Qt6_DIR}")
        # Из пути до каталога Qt6 поднимаемся на два уровня выше,
        # чтобы получить корневой префикс установки Qt.
        get_filename_component(_qt_prefix "$ENV{Qt6_DIR}/../.." ABSOLUTE)
        list(APPEND _qt_candidate_prefixes "${_qt_prefix}")
    endif()

    # Ищем каталоги Qt, установленные официальным установщиком Qt.
    # LIST_DIRECTORIES true говорит CMake возвращать именно директории.
    file(GLOB _qt_installer_prefixes LIST_DIRECTORIES true
        "/Applications/Qt/*/macos"
        "$ENV{HOME}/Qt/*/macos"
    )

    # Добавляем в список все реально существующие пути:
    # - найденные через Qt Installer;
    # - типовые префиксы Homebrew для разных компонентов Qt.
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

    # Если кандидаты найдены, добавляем их в CMAKE_PREFIX_PATH.
    # Именно этот список использует find_package(...) при поиске пакетов.
    if(_qt_candidate_prefixes)
        # PREPEND добавляет пути в начало списка, чтобы наши явные варианты
        # имели приоритет над прочими путями окружения.
        list(PREPEND CMAKE_PREFIX_PATH ${_qt_candidate_prefixes})
        list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
    endif()

    # Создаем отдельный список дополнительных корней Qt-пакетов.
    # Он пригодится для сценариев, когда части Qt лежат в смежных каталогах.
    set(_qt_additional_package_prefixes "")

    # Аналогично собираем только реально существующие директории.
    _append_existing_prefixes(
        _qt_additional_package_prefixes
        "/opt/homebrew/opt/qt"
        "/opt/homebrew/opt/qtsvg"
        "/opt/homebrew/opt/qtcharts"
        "/usr/local/opt/qt"
        "/usr/local/opt/qtsvg"
        "/usr/local/opt/qtcharts"
    )

    # Если список дополнительных префиксов не пустой,
    # сохраняем его в cache-переменную CMake.
    if(_qt_additional_package_prefixes)
        # list(JOIN ...) превращает список в строку с разделителем ';',
        # который CMake использует для хранения путевых списков.
        list(JOIN _qt_additional_package_prefixes ";" QT_ADDITIONAL_PACKAGES_PREFIX_PATH)

        # CACHE STRING делает переменную доступной между повторными
        # конфигурациями проекта.
        # FORCE обновляет значение даже если переменная уже существовала.
        set(QT_ADDITIONAL_PACKAGES_PREFIX_PATH
            "${QT_ADDITIONAL_PACKAGES_PREFIX_PATH}"
            CACHE STRING "Additional Qt package roots" FORCE)
    endif()
endif()

# ----------------------------------------------------------------------------
# ПОИСК НУЖНЫХ МОДУЛЕЙ QT
# ----------------------------------------------------------------------------
# REQUIRED означает, что без этих модулей конфигурация проекта должна
# завершиться ошибкой.
#
# В текущем приложении используются:
# - Widgets    : стандартный GUI на Qt Widgets;
# - Charts     : построение графиков;
# - Concurrent : выполнение расчетов вне GUI-потока.
# ----------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Widgets Charts Concurrent)

# ----------------------------------------------------------------------------
# ОБЪЯВЛЕНИЕ ИСПОЛНЯЕМОГО ФАЙЛА ПРОЕКТА
# ----------------------------------------------------------------------------
# Здесь перечисляются все основные составляющие целевого приложения:
# - главный исходный файл;
# - заголовочные файлы;
# - файлы реализации.
#
# MACOSX_BUNDLE означает, что на macOS приложение собирается как .app bundle.
# ----------------------------------------------------------------------------
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

# Подключаем папку include как каталог поиска заголовочных файлов проекта.
# PRIVATE означает, что этот путь нужен только самой цели ModelingMethodsLab2
# и не должен автоматически пробрасываться дальше.
target_include_directories(ModelingMethodsLab2 PRIVATE include)

# Просим компилятор использовать возможности стандарта C++17.
# Такой способ лучше directory-global переменной, потому что он привязан
# именно к конкретной цели.
target_compile_features(ModelingMethodsLab2 PRIVATE cxx_std_17)

# Передаем в код путь к корню проекта через compile definition.
# Это удобно, когда приложению нужно знать каталог исходников проекта,
# например для экспорта данных в подпапки рядом с проектом.
target_compile_definitions(
    ModelingMethodsLab2
    PRIVATE
        MODELING_METHODS_LAB2_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

# ----------------------------------------------------------------------------
# СВОЙСТВА ЦЕЛИ
# ----------------------------------------------------------------------------
# Здесь настраиваются автоматические механизмы Qt/CMake и параметры
# macOS bundle для конкретной цели приложения.
# ----------------------------------------------------------------------------
set_target_properties(
    ModelingMethodsLab2
    PROPERTIES
        # AUTOMOC включает автоматический запуск Meta-Object Compiler,
        # который нужен для классов Qt с Q_OBJECT.
        AUTOMOC ON
        # AUTORCC включает автоматическую обработку .qrc-файлов ресурсов Qt.
        AUTORCC ON
        # CXX_EXTENSIONS OFF запрещает GNU-расширения поверх стандарта,
        # чтобы код оставался ближе к чистому C++17.
        CXX_EXTENSIONS OFF
        # Имя .app bundle на macOS.
        MACOSX_BUNDLE_BUNDLE_NAME "ModelingMethodsLab2"
        # Bundle identifier приложения в стиле обратного домена.
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.modelingmethods.lab2"
)

# Линкуем приложение с теми библиотеками Qt, которые были найдены выше.
# Иначе на этапе компоновки целевой файл не получит реализации классов
# из Widgets, Charts и Concurrent.
target_link_libraries(ModelingMethodsLab2 PRIVATE Qt6::Widgets Qt6::Charts Qt6::Concurrent)
