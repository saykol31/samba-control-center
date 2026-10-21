cmake_minimum_required(VERSION 3.16)

project(SambaControlCenter
    VERSION 1.0.0
    DESCRIPTION "GTKmm desktop application for Samba administration"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)

add_executable(samba-control-center
    src/main.cpp
)

target_link_libraries(samba-control-center
    PRIVATE PkgConfig::GTKMM
)

include(GNUInstallDirs)

install(
    TARGETS samba-control-center
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
