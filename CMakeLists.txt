project(ptgrey_gstreamer)
cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR})
find_package(GStreamer COMPONENTS app)
find_package(GLIB)


#HACK for opensuse gstreamer
#TODO: move to findGStreamer.cmake
include_directories("/usr/lib64/gstreamer-1.0/include/")

find_path(FLYCAP_INCLUDE_DIR FlyCapture2.h PATH_SUFFIXES flycapture)
find_library(flycap flycapture)

find_library(gobject gobject-2.0)
message("flycap lib: ${flycap}")
message("gobject lib: ${gobject}")
message("flycap path: ${FLYCAP_INCLUDE_DIR}")
include_directories(${FLYCAP_INCLUDE_DIR})


include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIR})
include_directories(${GLIBCONFIG_INCLUDE_DIR})

set(SRCS ptgrey_gstreamer.cpp)
add_executable(ptgrey_gstreamer ${SRCS})

target_link_libraries(ptgrey_gstreamer ${gobject} ${GLIB_LIBRARIES} ${flycap} ${GSTREAMER_LIBRARIES} ${GSTREAMER_APP_LIBRARIES})
