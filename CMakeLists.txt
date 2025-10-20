cmake_minimum_required(VERSION 3.10)
project(rtsp_server C)
set(CMAKE_C_STANDARD 17)

# Buscar los paquetes necesarios
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.14)
pkg_check_modules(GST_RTSP REQUIRED gstreamer-rtsp-server-1.0)
pkg_check_modules(GST_VIDEO REQUIRED gstreamer-video-1.0)
pkg_check_modules(GST_RTSP REQUIRED gstreamer-rtsp-server-1.0)
pkg_check_modules(GST_RTSP_CORE REQUIRED gstreamer-rtsp-1.0)
pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0)
# Incluir los directorios encontrados por pkg-config
include_directories(
    ${GST_INCLUDE_DIRS}
    ${GST_RTSP_INCLUDE_DIRS}
    ${GST_VIDEO_INCLUDE_DIRS}
    ${GST_APP_INCLUDE_DIRS}
)

add_definitions(
    ${GST_CFLAGS_OTHER}
    ${GST_RTSP_CFLAGS_OTHER}
    ${GST_VIDEO_CFLAGS_OTHER}
    ${GST_APP_CFLAGS_OTHER}
)

# Añadir ejecutable sernder
add_executable(rtsp_server src/server.c)
target_link_libraries(rtsp_server
    ${GST_LIBRARIES}
    ${GST_RTSP_LIBRARIES}
    ${GST_RTSP_CORE_LIBRARIES}
    ${GST_VIDEO_LIBRARIES}
)
# Añadir el ejecutable receiver
add_executable(rtsp_viewer src/receiver.c)
target_link_libraries(rtsp_viewer ${GST_LIBRARIES} ${GST_RTSP_LIBRARIES} ${GST_VIDEO_LIBRARIES})

add_executable(usb_to_rtsp src/video_2_rtsp.c)
target_link_libraries(usb_to_rtsp ${GST_LIBRARIES} ${GST_RTSP_LIBRARIES} ${GST_VIDEO_LIBRARIES})

target_include_directories(rtsp_viewer PRIVATE ${GST_INCLUDE_DIRS})
target_compile_options(rtsp_viewer PRIVATE ${GST_CFLAGS_OTHER})
