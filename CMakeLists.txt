cmake_minimum_required(VERSION 3.10.2)

set (PROJ_NAME "PROFILE_LAYER")
project (VK_${PROJ_NAME})

set(API_NAME "Vulkan" CACHE STRING "API name")
add_definitions(-DAPI_NAME="${API_NAME}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CMakePrintHelpers)
cmake_print_variables(CMAKE_CURRENT_SOURCE_DIR)

# 1. Vulkan-Headers
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/Vulkan-Headers)
    set(VULKAN_HEADERS_INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/Vulkan-Headers)
endif()
if(NOT VULKAN_HEADERS_INSTALL_DIR)
    set(VULKAN_HEADERS_INSTALL_DIR $ENV{VULKAN_HEADERS_INSTALL_DIR})
endif()
message(STATUS "Using Vulkan-Headers install located at ${VULKAN_HEADERS_INSTALL_DIR}")
set(VULKAN_HEADERS_INCLUDE_DIR ${VULKAN_HEADERS_INSTALL_DIR}/include)
message(STATUS "Using Vulkan-Headers include dir located at ${VULKAN_HEADERS_INCLUDE_DIR}")

# 2. Vulkan-Loader
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/Vulkan-Loader)
    set(VULKAN_LOADER_INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/Vulkan-Loader)
endif()
if(NOT VULKAN_LOADER_INSTALL_DIR)
    set(VULKAN_LOADER_INSTALL_DIR $ENV{VULKAN_LOADER_INSTALL_DIR})
endif()
message(STATUS "Using Vulkan-Headers install located at ${VULKAN_LOADER_INSTALL_DIR}")

# 3. Vulkan-ValidationLayers
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/Vulkan-ValidationLayers)
    set(Vulkan-ValidationLayers_INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/Vulkan-ValidationLayers)
endif()
if(NOT Vulkan-ValidationLayers_INSTALL_DIR)
    set(Vulkan-ValidationLayers_INSTALL_DIR $ENV{Vulkan-ValidationLayers_INSTALL_DIR})
endif()
set(Vulkan-ValidationLayers_INCLUDE_DIR ${Vulkan-ValidationLayers_INSTALL_DIR}/include/vulkan)
message(STATUS "Using Vulkan-ValidationLayers_INCLUDE_DIR: ${Vulkan-ValidationLayers_INCLUDE_DIR}")

#set(VkLayer_utils_LIBRARY ${Vulkan-ValidationLayers_INSTALL_DIR}/lib/VkLayer_utils_dbg.lib)

message(STATUS "Using find_package to locate Vulkan")
find_package(Vulkan)
message(STATUS "Vulkan FOUND = ${Vulkan_FOUND}")
message(STATUS "Vulkan Include = ${Vulkan_INCLUDE_DIR}")
message(STATUS "Vulkan Lib = ${Vulkan_LIBRARY}")

IF (NOT Vulkan_FOUND)
	message(FATAL_ERROR "Could not find Vulkan library!")
ELSE()
	message(STATUS ${Vulkan_LIBRARY})
ENDIF()

find_package(PythonInterp 3 REQUIRED)
set (PYTHON_CMD ${PYTHON_EXECUTABLE})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME MATCHES "BSD")
    set(FALLBACK_CONFIG_DIRS "/etc/xdg" CACHE STRING
        "Search path to use when XDG_CONFIG_DIRS is unset or empty or the current process is SUID/SGID. Default is freedesktop compliant.")
    set(FALLBACK_DATA_DIRS "/usr/local/share:/usr/share" CACHE STRING
        "Search path to use when XDG_DATA_DIRS is unset or empty or the current process is SUID/SGID. Default is freedesktop compliant.")

    include(FindPkgConfig)
    option(BUILD_WSI_XCB_SUPPORT "Build XCB WSI support" ON)
    option(BUILD_WSI_XLIB_SUPPORT "Build Xlib WSI support" ON)
    option(BUILD_WSI_WAYLAND_SUPPORT "Build Wayland WSI support" ON)

    if (BUILD_WSI_XCB_SUPPORT)
        find_package(XCB REQUIRED)
    endif()

    if (BUILD_WSI_XLIB_SUPPORT)
        find_package(X11 REQUIRED)
    endif()

    if (BUILD_WSI_WAYLAND_SUPPORT)
        find_package(Wayland REQUIRED)
        include_directories(${WAYLAND_CLIENT_INCLUDE_DIR})
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    option(BUILD_MOLTENVK_SUPPORT "Build MoltenVK WSI support" ON)
    option(BUILD_METAL_SUPPORT "Build Metal WSI support" OFF)
endif()

set(VULKANTOOLS_SCRIPTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/scripts)
set(VulkanRegistry_DIR ${VULKANTOOLS_SCRIPTS_DIR}/registry)

# Define macro used for building vkxml generated files
macro(run_vulkantools_vk_xml_generate dependency output)
    add_custom_command(OUTPUT ${output}
    COMMAND ${PYTHON_CMD} -B ${VULKANTOOLS_SCRIPTS_DIR}/vt_genvk.py -registry ${VulkanRegistry_DIR}/vk.xml -scripts ${VulkanRegistry_DIR} ${output}
    DEPENDS ${VulkanRegistry_DIR}/vk.xml ${VulkanRegistry_DIR}/generator.py ${VULKANTOOLS_SCRIPTS_DIR}/${dependency} ${VULKANTOOLS_SCRIPTS_DIR}/vt_genvk.py ${VulkanRegistry_DIR}/reg.py
    )
endmacro()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR -DVK_USE_PLATFORM_WIN32_KHX -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
    set(DisplayServer Win32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    add_definitions(-DVK_USE_PLATFORM_ANDROID_KHR)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME MATCHES "BSD")
    if (BUILD_WSI_XCB_SUPPORT)
        add_definitions(-DVK_USE_PLATFORM_XCB_KHR -DVK_USE_PLATFORM_XCB_KHX)
    endif()

    if (BUILD_WSI_XLIB_SUPPORT)
       add_definitions(-DVK_USE_PLATFORM_XLIB_KHR -DVK_USE_PLATFORM_XLIB_KHX -DVK_USE_PLATFORM_XLIB_XRANDR_EXT)
    endif()

    if (BUILD_WSI_WAYLAND_SUPPORT)
       add_definitions(-DVK_USE_PLATFORM_WAYLAND_KHR -DVK_USE_PLATFORM_WAYLAND_KHX)
    endif()
else()
    message(FATAL_ERROR "Unsupported Platform!")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Vulkan_INCLUDE_DIR}
    ${VULKAN_HEADERS_INCLUDE_DIR}
    ${VULKAN_LOADER_INSTALL_DIR}
    ${VULKAN_LOADER_INSTALL_DIR}/generated
    ${Vulkan-ValidationLayers_INCLUDE_DIR}
)
link_directories(${Vulkan-ValidationLayers_INSTALL_DIR}/lib)

if (WIN32)
    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -D_CRT_SECURE_NO_WARNINGS")
    set (CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} -D_CRT_SECURE_NO_WARNINGS")
    set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -D_CRT_SECURE_NO_WARNINGS")
    set (CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -D_CRT_SECURE_NO_WARNINGS")
    set (CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -D_CRT_SECURE_NO_WARNINGS /bigobj")
    set (CMAKE_C_FLAGS_DEBUG     "${CMAKE_C_FLAGS_DEBUG} -D_CRT_SECURE_NO_WARNINGS /bigobj")
    # Turn off transitional "changed behavior" warning message for Visual Studio versions prior to 2015.
    # The changed behavior is that constructor initializers are now fixed to clear the struct members.
    add_compile_options("$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,19>>:/wd4351>")
else()
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wpointer-arith -Wno-unused-function -Wno-sign-compare")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpointer-arith -Wno-unused-function -Wno-sign-compare")
endif()

run_vulkantools_vk_xml_generate(layer_factory_generator.py layer_factory.h)
run_vulkantools_vk_xml_generate(layer_factory_generator.py layer_factory.cpp)
add_custom_target(generate_vlf DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/layer_factory.cpp ${CMAKE_CURRENT_BINARY_DIR}/layer_factory.h)
#set_target_properties(generate_vlf PROPERTIES FOLDER ${VULKANTOOLS_TARGET_FOLDER})

# Generates a list of subdirectories in a directory.  Used to pick up factory layers and interceptors
MACRO(SUBDIRLIST result curdir)
  FILE(GLOB children RELATIVE ${curdir} ${curdir}/*)
  SET(dirlist "")
  FOREACH(child ${children})
    IF(IS_DIRECTORY ${curdir}/${child})
      LIST(APPEND dirlist ${child})
    ENDIF()
  ENDFOREACH()
  SET(${result} ${dirlist})
ENDMACRO()

# Paths for the layer factory json template and the destination for factory layer json files
set (JSON_TEMPLATE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/layer_factory.json.in)
if (WIN32)
    set (JSON_DEST_PATH ${CMAKE_CURRENT_BINARY_DIR})
else()
    set (JSON_DEST_PATH ${CMAKE_CURRENT_BINARY_DIR})
endif()

set (CMAKE_LAYER_FACTORY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
SUBDIRLIST(ST_SUBDIRS ${CMAKE_LAYER_FACTORY_SOURCE_DIR})

# json file creation

file(GLOB BASE_SRC  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
file(GLOB BASE_HEADERS  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

message(STATUS "BASE_SRC = ${BASE_SRC}")

file(GLOB ValidationLayers_Src  ${Vulkan-ValidationLayers_INCLUDE_DIR}/*.c ${Vulkan-ValidationLayers_INCLUDE_DIR}/*.h)
message(STATUS "ValidationLayers_Src = ${ValidationLayers_Src}")

add_library(VkLayer_${PROJ_NAME} SHARED ${BASE_SRC} ${ValidationLayers_Src} ${CMAKE_CURRENT_BINARY_DIR}/layer_factory.cpp ${CMAKE_CURRENT_BINARY_DIR}/layer_factory.h VkLayer_${PROJ_NAME}.def)
set_source_files_properties(VkLayer_${PROJ_NAME}.def PROPERTIES HEADER_FILE_ONLY TRUE)
target_link_Libraries(VkLayer_${PROJ_NAME} ${VkLayer_utils_LIBRARY})
target_include_directories(VkLayer_${PROJ_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${Vulkan_INCLUDE_DIR})
