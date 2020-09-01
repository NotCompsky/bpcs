cmake_minimum_required(VERSION 3.9) # CMP0069 NEW

project(bpcs)

include(GNUInstallDirs)

option(ENABLE_STATIC "Build static executable, rather than linked" OFF)
option(NATIVE_MARCH  "Optimise for the native CPU? Will not run on other CPUs" ON)
option(BUILD_DOCS    "Build documentation" ON)
set(OPENCV_3RDPARTY_LIBDIR "/usr/local/lib64/opencv4/3rdparty" CACHE STRING "Path to 3rd party OpenCV libraries")

find_package(OpenCV REQUIRED)


set(COMPILER_FLAGS "-Os -s -frename-registers -fgcse-las -fno-stack-protector -funsafe-loop-optimizations -Wunsafe-loop-optimizations")
set(LINKER_FLAGS "-s")
if(NATIVE_MARCH)
	set(COMPILER_FLAGS "${COMPILER_FLAGS} -march=native")
endif()


set(LIBS)
if(ENABLE_STATIC)
	set(OPENCV_CORE_NAMES opencv_core.a libopencv_core.a)
	set(PNG_NAMES png.a libpng.a)
	find_library(ZLIB NAMES libz.a zlib.a)
	find_library(PTHREAD NAMES pthread.a pthreads.a libpthread.a libpthreads.a)
	find_library(ITT NAMES libittnotify.a HINTS "${OPENCV_3RDPARTY_LIBDIR}")
	set(LIBS "${ZLIB}" "${ITT}" "${PTHREAD}")
else()
	set(OPENCV_CORE_NAMES opencv_core)
	set(PNG_NAMES png)
endif()
find_library(OPENCV_CORE NAMES ${OPENCV_CORE_NAMES})
find_library(PNG NAMES ${PNG_NAMES})
set(LIBS "${OPENCV_CORE}" "${PNG}" "${LIBS}")

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")


add_executable(bpcs     "${SRC_DIR}/bpcs.cpp")
add_executable(bpcs-x   "${SRC_DIR}/bpcs.cpp")
add_executable(bpcs-fmt "${SRC_DIR}/fmt.cpp")
set_target_properties(
	bpcs
	bpcs-x
	bpcs-fmt
	PROPERTIES
		CXX_STANDARD 17
		LINK_FLAGS_RELEASE "${LINKER_FLAGS}"
		COMPILE_FLAGS "${COMPILER_FLAGS}"
)

include(CheckIPOSupported)
check_ipo_supported(RESULT is_ipo_supported)
if(is_ipo_supported)
	set_target_properties(
		bpcs
		bpcs-x
		bpcs-fmt
		PROPERTIES
			INTERPROCEDURAL_OPTIMIZATION TRUE
	)
else()
	message(WARNING "Compiler does not support IPO, so expect a larger binary size")
endif()

include_directories("/usr/local/include")
target_include_directories(bpcs   PRIVATE "${OpenCV_INCLUDE_DIRS}")
target_include_directories(bpcs-x PRIVATE "${OpenCV_INCLUDE_DIRS}")
target_link_libraries(bpcs   PRIVATE "${LIBS}")
target_link_libraries(bpcs-x PRIVATE "${LIBS}")
target_compile_definitions(bpcs PRIVATE "EMBEDDOR=1")
target_compile_definitions(bpcs-fmt PRIVATE "EMBEDDOR=1")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	target_compile_definitions(bpcs bpcs-x PRIVATE "TEST=1")
endif()

if(BUILD_DOCS)
	file(MAKE_DIRECTORY man)
	# Creates directory during build system generation
	set(DOCDIR "${CMAKE_CURRENT_SOURCE_DIR}/doc")
	add_custom_target(
		doc1 ALL
		COMMAND pandoc -s -t man "${DOCDIR}/bpcs.1.md" -o "man/bpcs.1"
	)
	add_custom_target(
		doc2 ALL
		COMMAND pandoc -s -t man "${DOCDIR}/bpcs.3.md" -o "man/bpcs.3"
	)
	add_custom_target(
		doc3 ALL
		COMMAND pandoc -s -t man "${DOCDIR}/bpcs-v.1.md" -o "man/bpcs-v.1"
	)
	add_custom_target(
		doc4 ALL
		COMMAND pandoc -s -t man "${DOCDIR}/bpcs-fmt.1.md" -o "man/bpcs-fmt.1"
	)
	install(FILES man/bpcs.1 man/bpcs.3 man/bpcs-v.1 man/bpcs-fmt.1 DESTINATION "${CMAKE_INSTALL_MANDIR}")
endif()


install(
	TARGETS bpcs bpcs-x bpcs-fmt
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
