cmake_minimum_required(VERSION 3.7)

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build Type")
option(ENABLE_STATIC "Build static executable, rather than linked" OFF)
option(NATIVE_MARCH  "Optimise for the native CPU? Will not run on other CPUs" ON)

find_package(OpenCV REQUIRED)

set(LIBS)
if(ENABLE_STATIC)
	set(OPENCV_CORE_NAMES opencv_core.a libopencv_core.a)
	set(PNG_NAMES png.a libpng.a)
	find_library(ZLIB libz.a zlib.a)
	find_library(PTHREAD pthread.a pthreads.a libpthread.a libpthreads.a)
	set(LIBS "${ZLIB}")# "${PTHREAD}")
else()
	set(OPENCV_CORE_NAMES opencv_core)
	set(PNG_NAMES png)
	set(ZLIB)
endif()
find_library(OPENCV_CORE NAMES ${OPENCV_CORE_NAMES})
find_library(PNG NAMES ${PNG_NAMES})
set(LIBS "${OPENCV_CORE}" "${PNG}" "${LIBS}")

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(COMPILER_FLAGS "-Os -frename-registers -fgcse-las -fno-stack-protector -funsafe-loop-optimizations -Wunsafe-loop-optimizations")
if(NATIVE_MARCH)
	set(COMPILER_FLAGS "${COMPILER_FLAGS} -march=native")
endif()


file(MAKE_DIRECTORY "${AUTO_SRC_DIR}")
# Creates directory during build system generation

add_executable(bpcs     "${SRC_DIR}/bpcs.cpp")
add_executable(bpcs-x   "${SRC_DIR}/bpcs.cpp")
add_executable(bpcs-fmt "${SRC_DIR}/fmt.cpp")
set_target_properties(
	bpcs
	bpcs-x
	bpcs-fmt
	PROPERTIES
		CXX_STANDARD 17
		INTERPROCEDURAL_OPTIMIZATION TRUE
		LINK_FLAGS_RELEASE -s
		COMPILE_FLAGS "${COMPILER_FLAGS}"
)
target_include_directories(bpcs   PRIVATE "${OpenCV_INCLUDE_DIR}")
target_include_directories(bpcs-x PRIVATE "${OpenCV_INCLUDE_DIR}")
target_link_libraries(bpcs   PRIVATE "${LIBS}")
target_link_libraries(bpcs-x PRIVATE "${LIBS}")
target_compile_definitions(bpcs PRIVATE "EMBEDDOR=1")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	target_compile_definitions(bpcs bpcs-x PRIVATE "TEST=1")
endif()