cmake_minimum_required(VERSION 3.5)

project(camera_drive VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#头文件目录
include_directories(${camera_drive_SOURCE_DIR}/ffmpeg-6.1/include)
include_directories(${camera_drive_SOURCE_DIR}/SDL2-2.26.5/include)
include_directories(${camera_drive_SOURCE_DIR}/OpenSSL-Win64/include)
include_directories(${camera_drive_SOURCE_DIR}/curl-8.2.1/builds/libcurl-vc17-x64-debug-dll-ipv6-sspi-schannel/include)

#添加需要链接的库文件目录，相当于g++命令的-L选项的作用
LINK_DIRECTORIES(${camera_drive_SOURCE_DIR}/ffmpeg-6.1/lib)
LINK_DIRECTORIES(${camera_drive_SOURCE_DIR}/SDL2-2.26.5/lib/x64)
LINK_DIRECTORIES(${camera_drive_SOURCE_DIR}/curl-8.2.1/builds/libcurl-vc17-x64-debug-dll-ipv6-sspi-schannel/lib)
LINK_DIRECTORIES(${camera_drive_SOURCE_DIR}/OpenSSL-Win64/lib)

#添加需要链接的库文件，要用在add_executable之前
LINK_LIBRARIES(avcodec avdevice avfilter avformat avutil postproc swresample swscale)
LINK_LIBRARIES(SDL2 SDL2main libcurl_debug libssl libcrypto openssl)

#cmake的时候输出
MESSAGE(STATUS "This is BINARY dir " ${camera_drive_BINARY_DIR})
MESSAGE(STATUS "This is SOURCE dir " ${camera_drive_SOURCE_DIR})

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(camera_drive
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        mainwindow.h mainwindow.cpp mainwindow.ui
        login.h login.cpp login.ui
        voiceinfo.h voiceinfo.cpp
        imageinfo.h imageinfo.cpp
        utils.h utils.cpp
        sdllabel.h sdllabel.cpp
        mylabel.h mylabel.cpp
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET camera_drive APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(camera_drive SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(camera_drive
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(camera_drive PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.camera_drive)
endif()
set_target_properties(camera_drive PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS camera_drive
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(camera_drive)
endif()
