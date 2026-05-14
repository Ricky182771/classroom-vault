cmake_minimum_required(VERSION 3.20)

project(ClassroomVault VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets)

qt_standard_project_setup()

qt_add_executable(classroom-vault
    src/main.cpp
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
    src/SyncStagingManager.hpp
    src/SyncStagingManager.cpp
    src/SyncDiffEngine.hpp
    src/SyncDiffEngine.cpp
    src/DriveClient.hpp
    src/DriveClient.cpp
    src/AttachmentDownloader.hpp
    src/AttachmentDownloader.cpp
    src/AttachmentChecksumManager.hpp
    src/AttachmentChecksumManager.cpp
    src/Models.hpp
    src/Utils.hpp
    src/ui/MainWindow.hpp
    src/ui/MainWindow.cpp
    src/ui/StyleManager.hpp
    src/ui/StyleManager.cpp
    src/ui/UiModels.hpp
    src/ui/BreadcrumbWidget.hpp
    src/ui/BreadcrumbWidget.cpp
    src/ui/HomeDashboardWidget.hpp
    src/ui/HomeDashboardWidget.cpp
    src/ui/CourseDetailWidget.hpp
    src/ui/CourseDetailWidget.cpp
    src/ui/AssignmentListItemWidget.hpp
    src/ui/AssignmentListItemWidget.cpp
    src/ui/AssignmentStatusResolver.hpp
    src/ui/AssignmentStatusResolver.cpp
    src/ui/AssignmentDetailWidget.hpp
    src/ui/AssignmentDetailWidget.cpp
    src/ui/UserWorkPanelWidget.hpp
    src/ui/UserWorkPanelWidget.cpp
    src/ui/AttachmentCardWidget.hpp
    src/ui/AttachmentCardWidget.cpp
    src/ui/ActivityDrawerWidget.hpp
    src/ui/ActivityDrawerWidget.cpp
    src/ui/MetadataReader.hpp
    src/ui/MetadataReader.cpp
    src/ui/SidebarWidget.hpp
    src/ui/SidebarWidget.cpp
    src/ui/TopBarWidget.hpp
    src/ui/TopBarWidget.cpp
    src/ui/PathBarWidget.hpp
    src/ui/PathBarWidget.cpp
    src/ui/DashboardWidget.hpp
    src/ui/DashboardWidget.cpp
    src/ui/HeroStripWidget.hpp
    src/ui/HeroStripWidget.cpp
    src/ui/KpiCardWidget.hpp
    src/ui/KpiCardWidget.cpp
    src/ui/CourseGridWidget.hpp
    src/ui/CourseGridWidget.cpp
    src/ui/CourseCardWidget.hpp
    src/ui/CourseCardWidget.cpp
    src/ui/ActivityPanelWidget.hpp
    src/ui/ActivityPanelWidget.cpp
    src/ui/StorageCardWidget.hpp
    src/ui/StorageCardWidget.cpp
    src/ui/StatusBarWidget.hpp
    src/ui/StatusBarWidget.cpp
    src/ui/TasksViewWidget.hpp
    src/ui/TasksViewWidget.cpp
    src/ui/AttachmentsViewWidget.hpp
    src/ui/AttachmentsViewWidget.cpp
    src/ui/HistoryViewWidget.hpp
    src/ui/HistoryViewWidget.cpp
    src/ui/SettingsViewWidget.hpp
    src/ui/SettingsViewWidget.cpp
    resources/app.qrc
)

target_link_libraries(classroom-vault
    PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Widgets
)
