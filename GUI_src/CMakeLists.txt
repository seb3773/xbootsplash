cmake_minimum_required(VERSION 3.10)
project(xbootsplash-gui C CXX)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Aggressive optimizations preferred by user
option(XBOOTSPLASH_AGGRESSIVE_FLAGS "Enable aggressive -O2/LTO compile flags" ON)

if(XBOOTSPLASH_AGGRESSIVE_FLAGS)
    set(_XBS_C_FLAGS "-O2 -flto=auto -fstrict-aliasing -ffunction-sections -fdata-sections -fno-ident -fno-plt -fomit-frame-pointer -fno-stack-protector -fno-math-errno -fvisibility=hidden -fmerge-all-constants -fno-asynchronous-unwind-tables -DNDEBUG")
    set(_XBS_CXX_EXTRA "-std=c++03 -fno-threadsafe-statics -fno-exceptions -fmerge-all-constants -Wno-deprecated-declarations -fpermissive")
    set(CMAKE_C_FLAGS "${_XBS_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${_XBS_C_FLAGS} ${_XBS_CXX_EXTRA}")
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,-O1 -Wl,--gc-sections -Wl,--as-needed -Wl,--build-id=none -Wl,-z,norelro -flto=auto -s")
    set(CMAKE_C_FLAGS_DEBUG "")
    set(CMAKE_C_FLAGS_RELEASE "")
    set(CMAKE_CXX_FLAGS_DEBUG "")
    set(CMAKE_CXX_FLAGS_RELEASE "")
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    endif()
else()
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-std=c++03>)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(TQT REQUIRED tqt-mt)

find_program(TQMOC_EXECUTABLE NAMES tqmoc moc-tqt)
if (NOT TQMOC_EXECUTABLE)
    message(FATAL_ERROR "tqmoc or moc-tqt not found!")
endif()

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${TQT_INCLUDE_DIRS}
    /usr/include/tqt3
    /usr/include/tqt
    /opt/trinity/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/collapsible-group
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/toggle-buttons
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/message-log-widget
    ${CMAKE_CURRENT_SOURCE_DIR}/../zx0
    ${CMAKE_CURRENT_SOURCE_DIR}/../upkr
)

# Macro for generating .moc inclusion files
macro(GEN_TQT_MOC_INCL infile outfile)
    add_custom_command(
        OUTPUT ${outfile}
        COMMAND ${TQMOC_EXECUTABLE} ${infile} -o ${outfile}
        DEPENDS ${infile}
    )
endmacro()

# Macro for generating moc_*.cpp compilation files
macro(GEN_TQT_MOC_SRC out_list infile outfile)
    add_custom_command(
        OUTPUT ${outfile}
        COMMAND ${TQMOC_EXECUTABLE} ${infile} -o ${outfile}
        DEPENDS ${infile}
    )
    list(APPEND ${out_list} ${outfile})
endmacro()

# 1. Included MOCs (.moc files)
gen_tqt_moc_incl(${CMAKE_CURRENT_SOURCE_DIR}/libs/collapsible-group/tqtcollapsiblegroup.h ${CMAKE_CURRENT_BINARY_DIR}/tqtcollapsiblegroup.moc)
gen_tqt_moc_incl(${CMAKE_CURRENT_SOURCE_DIR}/src/preview_widget.h ${CMAKE_CURRENT_BINARY_DIR}/preview_widget.moc)
gen_tqt_moc_incl(${CMAKE_CURRENT_SOURCE_DIR}/src/fullscreen_preview.h ${CMAKE_CURRENT_BINARY_DIR}/fullscreen_preview.moc)
gen_tqt_moc_incl(${CMAKE_CURRENT_SOURCE_DIR}/src/config_panel.h ${CMAKE_CURRENT_BINARY_DIR}/config_panel.moc)
gen_tqt_moc_incl(${CMAKE_CURRENT_SOURCE_DIR}/src/build_engine.h ${CMAKE_CURRENT_BINARY_DIR}/build_engine.moc)
gen_tqt_moc_incl(${CMAKE_CURRENT_SOURCE_DIR}/src/package_manager.h ${CMAKE_CURRENT_BINARY_DIR}/package_manager.moc)
gen_tqt_moc_incl(${CMAKE_CURRENT_SOURCE_DIR}/src/installer_engine.h ${CMAKE_CURRENT_BINARY_DIR}/installer_engine.moc)
gen_tqt_moc_incl(${CMAKE_CURRENT_SOURCE_DIR}/src/password_dialog.h ${CMAKE_CURRENT_BINARY_DIR}/password_dialog.moc)
gen_tqt_moc_incl(${CMAKE_CURRENT_SOURCE_DIR}/src/silent_boot_dialog.h ${CMAKE_CURRENT_BINARY_DIR}/silent_boot_dialog.moc)
gen_tqt_moc_incl(${CMAKE_CURRENT_SOURCE_DIR}/src/package_inspector_dialog.h ${CMAKE_CURRENT_BINARY_DIR}/package_inspector_dialog.moc)
gen_tqt_moc_incl(${CMAKE_CURRENT_SOURCE_DIR}/src/about_dialog.h ${CMAKE_CURRENT_BINARY_DIR}/about_dialog.moc)
gen_tqt_moc_incl(${CMAKE_CURRENT_SOURCE_DIR}/src/user_guide_dialog.h ${CMAKE_CURRENT_BINARY_DIR}/user_guide_dialog.moc)
gen_tqt_moc_incl(${CMAKE_CURRENT_SOURCE_DIR}/src/mainwindow.h ${CMAKE_CURRENT_BINARY_DIR}/mainwindow.moc)

set(MOC_INCL_FILES
    ${CMAKE_CURRENT_BINARY_DIR}/tqtcollapsiblegroup.moc
    ${CMAKE_CURRENT_BINARY_DIR}/preview_widget.moc
    ${CMAKE_CURRENT_BINARY_DIR}/fullscreen_preview.moc
    ${CMAKE_CURRENT_BINARY_DIR}/config_panel.moc
    ${CMAKE_CURRENT_BINARY_DIR}/build_engine.moc
    ${CMAKE_CURRENT_BINARY_DIR}/package_manager.moc
    ${CMAKE_CURRENT_BINARY_DIR}/installer_engine.moc
    ${CMAKE_CURRENT_BINARY_DIR}/password_dialog.moc
    ${CMAKE_CURRENT_BINARY_DIR}/silent_boot_dialog.moc
    ${CMAKE_CURRENT_BINARY_DIR}/package_inspector_dialog.moc
    ${CMAKE_CURRENT_BINARY_DIR}/about_dialog.moc
    ${CMAKE_CURRENT_BINARY_DIR}/user_guide_dialog.moc
    ${CMAKE_CURRENT_BINARY_DIR}/mainwindow.moc
)

# 2. Standalone MOC sources (moc_*.cpp)
set(STANDALONE_MOC_SRCS)
gen_tqt_moc_src(STANDALONE_MOC_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/libs/toggle-buttons/tqttoggleswitch.h ${CMAKE_CURRENT_BINARY_DIR}/moc_tqttoggleswitch.cpp)
gen_tqt_moc_src(STANDALONE_MOC_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/libs/message-log-widget/tqtmessagelogwidget.h ${CMAKE_CURRENT_BINARY_DIR}/moc_tqtmessagelogwidget.cpp)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/about_dialog.cpp
    src/user_guide_dialog.cpp
    src/app_icons.cpp
    src/preview_widget.cpp
    src/fullscreen_preview.cpp
    src/config_panel.cpp
    src/build_engine.cpp
    src/embedded_engine_extractor.cpp
    src/package_manager.cpp
    src/package_inspector_dialog.cpp
    src/installer_engine.cpp
    src/password_dialog.cpp
    src/silent_boot_dialog.cpp
    src/vt_runner_helper.cpp
    libs/collapsible-group/tqtcollapsiblegroup.cpp
    libs/toggle-buttons/tqttoggleswitch.cpp
    libs/message-log-widget/tqtmessagelogwidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../zx0/zx0_decompress.c
    ${STANDALONE_MOC_SRCS}
    ${MOC_INCL_FILES}
)

add_executable(xbs_vt_runner src/xbs_vt_runner.c)
set_target_properties(xbs_vt_runner PROPERTIES
    C_STANDARD 99
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/embedded_vt_runner.h
    COMMAND /bin/sh -c "xxd -i xbs_vt_runner | sed 's/unsigned char/static const unsigned char/' | sed 's/unsigned int/static const unsigned int/' > ${CMAKE_CURRENT_BINARY_DIR}/embedded_vt_runner.h"
    DEPENDS xbs_vt_runner
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Embedding stripped xbs_vt_runner into C++ header"
    VERBATIM
)

add_executable(pack_engine tools/pack_engine.c ${CMAKE_CURRENT_SOURCE_DIR}/../zx0/zx0_compress.c)
set_target_properties(pack_engine PROPERTIES
    C_STANDARD 99
)
target_include_directories(pack_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../zx0 ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/embedded_engine.h
    COMMAND pack_engine ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/embedded_engine.h
    DEPENDS pack_engine
            ${CMAKE_CURRENT_SOURCE_DIR}/../engine_src/generate_splash.c
            ${CMAKE_CURRENT_SOURCE_DIR}/../engine_src/splash_anim_delta.c
            ${CMAKE_CURRENT_SOURCE_DIR}/../engine_src/splash_anim_drm.c
            ${CMAKE_CURRENT_SOURCE_DIR}/../engine_src/nolibc.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../engine_src/start.S
            ${CMAKE_CURRENT_SOURCE_DIR}/../engine_src/linker.ld
            ${CMAKE_CURRENT_SOURCE_DIR}/../Makefile
            ${CMAKE_CURRENT_SOURCE_DIR}/../zx0/zx0_compress.c
            ${CMAKE_CURRENT_SOURCE_DIR}/../zx0/zx0_compress.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../zx0/zx0_decompress.c
            ${CMAKE_CURRENT_SOURCE_DIR}/../zx0/zx0_decompress.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../upkr/upkr_compress.c
            ${CMAKE_CURRENT_SOURCE_DIR}/../upkr/upkr_compress.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../upkr/upkr_decompress.c
            ${CMAKE_CURRENT_SOURCE_DIR}/../upkr/upkr_decompress.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../icons/resume.png
            ${CMAKE_CURRENT_SOURCE_DIR}/../icons/resume_hd.png
    COMMENT "Packing and ZX0-compressing engine sources into embedded_engine.h"
    VERBATIM
)

add_executable(xbootsplash-gui ${SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/embedded_vt_runner.h ${CMAKE_CURRENT_BINARY_DIR}/embedded_engine.h)
target_link_libraries(xbootsplash-gui ${TQT_LIBRARIES})
add_dependencies(xbootsplash-gui xbs_vt_runner pack_engine)
