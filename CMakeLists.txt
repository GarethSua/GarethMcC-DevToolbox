cmake_minimum_required(VERSION 3.16)
project(DeveloperToolboxCMake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(qt.cmake)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
        Concurrent
)
qt_standard_project_setup()

set(PROJECT_SOURCES
    main.cpp
    MainWindow.ui
    MainWindow.hpp
    MainWindow.cpp

    PasswordGenerator.hpp
    PasswordGenerator.cpp

    HashCalculator.hpp
    HashCalculator.cpp

    JsonFormatter.hpp
    JsonFormatter.cpp

    TextSearcher.hpp
    TextSearcher.cpp

    DuplicateFileFinder.hpp
    DuplicateFileFinder.cpp

    LogViewer.hpp
    LogViewer.cpp

    SystemMonitor.hpp
    SystemMonitor.cpp

    FolderSynchroniser.hpp
    FolderSynchroniser.cpp
)

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        WIN32_EXECUTABLE TRUE
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Concurrent
        dxgi
)

