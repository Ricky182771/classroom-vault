cmake_minimum_required(VERSION 3.20)

project(ClassroomVault VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets)

qt_standard_project_setup()

qt_add_executable(classroom-vault
    src/main.cpp
    src/MainWindow.hpp
    src/MainWindow.cpp
    src/GoogleAuth.hpp
    src/GoogleAuth.cpp
    src/ClassroomClient.hpp
    src/ClassroomClient.cpp
    src/FolderOrganizer.hpp
    src/FolderOrganizer.cpp
    src/ConfigManager.hpp
    src/ConfigManager.cpp
    src/SyncManager.hpp
    src/SyncManager.cpp
    src/SyncStateManager.hpp
    src/SyncStateManager.cpp
    src/DriveClient.hpp
    src/DriveClient.cpp
    src/AttachmentDownloader.hpp
    src/AttachmentDownloader.cpp
    src/Models.hpp
    src/Utils.hpp
    resources/app.qrc
)

target_link_libraries(classroom-vault
    PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Widgets
)
