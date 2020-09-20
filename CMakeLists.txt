cmake_minimum_required(VERSION 3.9) # CMP0069 NEW

project(bpcs)

include(GNUInstallDirs)

option(ENABLE_STATIC "Build static executable, rather than linked" OFF)
option(NATIVE_MARCH  "Optimise for the native CPU? Will not run on other CPUs" ON)
option(BUILD_DOCS    "Build documentation" OFF)
option(ENABLE_EXCEPTS "Enable exceptions" ON)
option(CHITTY_CHATTY "Be very verbose" OFF)
option(ENABLE_RUNTIME_TESTS  "Enable runtime tests" ON)
option(AGGRESSIVE_DEAD_CODE_REMOVAL "Aggressively purge dead code, structuring the resulting binary in a form which may be slower. Does not, however, appear to have any affect." OFF)
option(UNSAFE_OPTIMISATIONS "Enable unsafe optimisations, such as -funsafe-loop-optimizations" OFF)
set(MALLOC_OVERRIDE "" CACHE STRING "Path to object file that overrides the malloc implementation")
set(GRID_W 9 CACHE STRING "Grid width")
set(GRID_H 9 CACHE STRING "Grid height")
set(N_CHANNELS 3 CACHE STRING "Number of colour channels in each image")
set(MAX_BIT_DEPTH 32 CACHE STRING "Maximum bit-depth of images used. There is not much performance loss from higher values.")
set(MAX_FILE_PATH_LEN 1024 CACHE STRING "Maximum file path length")

set(COMPILER_FLAGS "-Os -s -frename-registers -fgcse-las -fno-stack-protector -funsafe-loop-optimizations -Wunsafe-loop-optimizations -Wno-trigraphs")
set(LINKER_FLAGS "-s")
if(NATIVE_MARCH)
	set(COMPILER_FLAGS "${COMPILER_FLAGS} -march=native")
endif()
if("${CMAKE_BUILD_TYPE}" STREQUAL Debug)
	if(WIN32)
		set(COMPILER_FLAGS "${COMPILER_FLAGS} -gstabs")
	endif()
endif()


if(WIN32)
	set(ENABLE_STATIC ON)
endif()


set(LIBS)
if(ENABLE_STATIC)
	set(PNG_NAMES png.a libpng.a)
	find_library(ZLIB NAMES libz.a zlib.a)
	find_library(PTHREAD NAMES pthread.a pthreads.a libpthread.a libpthreads.a)
	set(LIBS "${ZLIB}" "${PTHREAD}")
else()
	set(PNG_NAMES png)
endif()
find_library(PNG NAMES ${PNG_NAMES})
set(LIBS "${PNG}" "${LIBS}")

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(MALLOC_OBJECTS)
if(NOT MALLOC_OVERRIDE STREQUAL "")
	set(MALLOC_OBJECTS "${MALLOC_OVERRIDE}")
endif()

add_executable(bpcs-fmt ${MALLOC_OBJECTS} "${SRC_DIR}/fmt.cpp" "${SRC_DIR}/fmt_os.cpp")

include_directories("/usr/local/include")

foreach(tgt bpcs bpcs-x bpcs-count)
	add_executable("${tgt}" ${MALLOC_OBJECTS} "${SRC_DIR}/bpcs.cpp" "${SRC_DIR}/os.cpp" "${SRC_DIR}/main.cpp")
	target_compile_definitions("${tgt}" PRIVATE GRID_W=${GRID_W} GRID_H=${GRID_H} N_CHANNELS=${N_CHANNELS} MAX_BITPLANES=${MAX_BIT_DEPTH} MAX_FILE_PATH_LEN=${MAX_FILE_PATH_LEN})
	target_include_directories("${tgt}" PRIVATE "${OpenCV_INCLUDE_DIRS}")
	target_link_libraries("${tgt}" PRIVATE "${LIBS}")
endforeach()

foreach(tgt bpcs bpcs-x bpcs-count bpcs-fmt)
	if(ENABLE_RUNTIME_TESTS)
		target_compile_definitions("${tgt}" PRIVATE TESTS)
	endif()
	if(NOT ENABLE_EXCEPTS)
		target_compile_definitions("${tgt}" PRIVATE NO_EXCEPTIONS)
	endif()
	if(CHITTY_CHATTY)
		target_compile_definitions("${tgt}" PRIVATE CHITTY_CHATTY)
	endif()
endforeach()


target_compile_definitions(bpcs PRIVATE EMBEDDOR)
target_compile_definitions(bpcs-fmt PRIVATE EMBEDDOR)
target_compile_definitions(bpcs-count PRIVATE ONLY_COUNT)

if(AGGRESSIVE_DEAD_CODE_REMOVAL)
	set(COMPILER_FLAGS "${COMPILER_FLAGS} -fdata-sections -ffunction-sections")
	set(LINKER_FLAGS "${LINKER_FLAGS} --gc-sections")
endif()
if(UNSAFE_OPTIMISATIONS)
	set(COMPILER_FLAGS "${COMPILER_FLAGS} -ffast-math")
endif()

set_target_properties(
	bpcs
	bpcs-x
	bpcs-fmt
	bpcs-count
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
		bpcs-count
		PROPERTIES
			INTERPROCEDURAL_OPTIMIZATION TRUE
	)
else()
	message(WARNING "Compiler does not support IPO, so expect a larger binary size")
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
	TARGETS bpcs bpcs-x bpcs-fmt bpcs-count
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
