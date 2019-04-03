cmake_minimum_required(VERSION 3.0)

function(get_version_from_ac var file_name)
    file(STRINGS "${file_name}" ac_init_string REGEX "^AC_INIT")
    string(REGEX REPLACE "^AC_INIT\\(\\[.*\\], \\[([0-9]+)\\], \\[.*\\]\\)" "\\1" ac_version ${ac_init_string})
    set(${var} ${ac_version} PARENT_SCOPE)
endfunction()

get_version_from_ac(LIBTEXPDF_VERSION configure.ac)

include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CheckStructHasMember)
include(CheckLibraryExists)

project(libtexpdf VERSION ${LIBTEXPDF_VERSION} LANGUAGES C)

if (WIN32)
	include(ExternalProject)

	set(TMP_INSTALL_DIR "${CMAKE_BINARY_DIR}/tmp_install")

	ExternalProject_Add(zlib
		GIT_REPOSITORY https://github.com/madler/zlib
		GIT_TAG v1.2.11
		CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
		INSTALL_DIR "${TMP_INSTALL_DIR}")

	# Never use dynamic version of zlib. workaround by replace zlib with zlibstatic
	ExternalProject_Add_Step(zlib replace_lib
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TMP_INSTALL_DIR}/lib/zlibstatic.lib" "${TMP_INSTALL_DIR}/lib/zlib.lib"
		DEPENDEES install)

	ExternalProject_Add(libpng
		DEPENDS zlib
		GIT_REPOSITORY https://github.com/glennrp/libpng
		GIT_TAG v1.6.36
		CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>" "-DZLIB_ROOT=${TMP_INSTALL_DIR}"
		INSTALL_DIR "${TMP_INSTALL_DIR}")

	set(HAVE_ZLIB 1)
	set(HAVE_ZLIB_COMPRESS2 1)
	set(HAVE_LIBPNG 1)

else()
	find_package(PNG)
	find_package(ZLIB)

	if (ZLIB_FOUND)
		set(HAVE_ZLIB 1)
		set(HAVE_ZLIB_COMPRESS2 1)
	endif()
	if (PNG_FOUND)
		set(HAVE_LIBPNG 1)
	endif()
endif()

# Checks for header files.
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(memory.h HAVE_MEMORY_H)
check_include_file(stdbool.h HAVE_STDBOOL_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(sys/wait.h HAVE_SYS_WAIT_H)
check_include_file(sys/stdbool.h HAVE_SYS_STDBOOL_H)

# Checks for library functions.
check_function_exists(getenv HAVE_GETENV)
check_function_exists(mkstemp HAVE_MKSTEMP)

# Checks for typedefs, structures, and compiler characteristics.
check_symbol_exists(timezone time.h HAVE_TIMEZONE)
check_struct_has_member("struct tm" tm_gmtoff time.h HAVE_TM_GMTOFF)

check_type_size(long SIZEOF_LONG)

set(PACKAGE_NAME "\"${PROJECT_NAME}\"")
set(PACKAGE_VERSION "\"${LIBTEXPDF_VERSION}\"")

configure_file(config.h.cmake config.h)

file(GLOB SRC_FILES *.c)
file(GLOB HDR_FILES *.h)
set(TEST_SRC library-poc.c)
list(REMOVE_ITEM SRC_FILES ${TEST_SRC})

add_library(libtexpdf STATIC ${SRC_FILES} ${HDR_FILES})
add_dependencies(libtexpdf zlib libpng)
target_compile_definitions(libtexpdf PUBLIC HAVE_CONFIG_H=1 CDECL=)
target_compile_definitions(libtexpdf PRIVATE BUILDING_LIBTEXPDF=1)
if (WIN32)
	target_include_directories(libtexpdf PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" "${TMP_INSTALL_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/win32")
	target_link_directories(libtexpdf PUBLIC "${TMP_INSTALL_DIR}/lib")
	target_link_libraries(libtexpdf PUBLIC optimized zlibstatic debug zlibstaticd)
	target_link_libraries(libtexpdf PUBLIC optimized libpng16_static debug libpng16_staticd)
else()
	target_link_libraries(libtexpdf PUBLIC ZLIB::ZLIB PNG::PNG)
endif()

add_executable(libtexpdf_test ${TEST_SRC})
target_link_libraries(libtexpdf_test PUBLIC libtexpdf)
