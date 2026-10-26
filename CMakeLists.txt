cmake_minimum_required(VERSION 3.16)

project(SambaControlCenter
    VERSION 2.0.0
    DESCRIPTION "GTKmm modular desktop application for Samba administration"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)

add_executable(samba-control-center
    src/main.cpp
    src/main_window.cpp
    src/home_page.cpp
    src/control_page.cpp
    src/legacy_control.cpp
    src/user_page.cpp
    src/settings_page.cpp
    src/shared_status.cpp
)

target_include_directories(samba-control-center PRIVATE include)

target_link_libraries(samba-control-center
    PRIVATE PkgConfig::GTKMM
)

target_compile_options(samba-control-center PRIVATE
    -Wall -Wextra -Wpedantic
)

include(GNUInstallDirs)

install(
    TARGETS samba-control-center
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
