cmake_minimum_required(VERSION 2.8)
PROJECT(RAYTRACER)
SET(CMAKE_BUILD_TYPE "Release")

if (!MSVC)
SET(CMAKE_CXX_FLAGS "-Wno-deprecated")
endif()


#Compile and Link GLFW
ADD_SUBDIRECTORY(vendor/glfw-3.2)
link_libraries(glfw)
include_directories(${glfw_INCLUDE_DIRS})
include_directories("${CMAKE_SOURCE_DIR}/vendor/glfw-3.2/deps")

add_library(glad "${CMAKE_SOURCE_DIR}/vendor/glfw-3.2/deps/glad/glad.h"
         		 "${CMAKE_SOURCE_DIR}/vendor/glfw-3.2/deps/glad.c")
link_libraries(glad)

ADD_SUBDIRECTORY(vendor/pngdecode)
link_libraries(pngdecode)
include_directories(${CMAKE_SOURCE_DIR}/vendor/pngdecode)

SET(MY_SOURCE_PATH ${CMAKE_SOURCE_DIR})
CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/source/common/SourcePath.cpp.in ${CMAKE_SOURCE_DIR}/source/common/SourcePath.cpp)	

include_directories(${CMAKE_SOURCE_DIR}/source/common
					${CMAKE_SOURCE_DIR}/shaders)
					


					
add_executable(raytracer WIN32 MACOSX_BUNDLE 
	source/main.cpp 
	source/common/stb_image.h
	source/common/std_image.cpp
	source/common/common.h
    source/common/BoundingVolumeHierarchyNode.h
	source/common/CheckError.h
	source/common/mat.h
	source/common/ObjMesh.cpp
	source/common/ObjMesh.h
	source/common/SourcePath.cpp
	source/common/SourcePath.h
	source/common/Trackball.cpp
	source/common/Trackball.h
	source/common/Object.cpp
	source/common/Object.h
	source/common/vec.h
	assets/shaders/fshader.glsl
	assets/shaders/vshader.glsl)

find_package(OpenMP)
if ((OpenMP_CXX_FOUND) OR (OpenMP_FOUND))
  if (CMAKE_VERSION VERSION_GREATER "3.7")
    target_link_libraries (raytracer OpenMP::OpenMP_CXX)
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
  endif()
  message("OpenMP Found")
else()
  message("OpenMP NOT Found")
endif()

    
#Windows cleanup
if (MSVC)
    # Tell MSVC to use main instead of WinMain for Windows subsystem executables
    set_target_properties(raytracer PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif()

#Apple cleanup
if (APPLE)
    set_target_properties(raytracer PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "simple")
	
    set_target_properties(raytracer PROPERTIES
                          MACOSX_BUNDLE_SHORT_VERSION_STRING ${GLFW_VERSION}
                          MACOSX_BUNDLE_LONG_VERSION_STRING ${GLFW_VERSION_FULL}
    					  MACOSX_BUNDLE_ICON_FILE glfw.icns)				  
endif()
