cmake_minimum_required(VERSION 3.25)

project(DemoProject VERSION 1.07.00)

set(Package_maintainer "Egil Rasmussen")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 26)

option(RELEASE_BUILD "Release build " OFF)
option(BUILD_UNITTEST "Build unittests " OFF)
option(ENABLE_CCACHE "Enable ccache" OFF)
option(BUILD_SHARED  "Build as shared library" OFF)
option(ENABLE_SANITIZER "Enable address sanitizer" OFF)
option(BUILD_FUZZER "Build fuzzer" OFF)
option(BUILD_BENCHMARK "Build benchmarking test" OFF)
option(RUN_CLANG_TIDY "Run clang-tidy" OFF)
option(ENABLE_CPPCHECK "Enable cppcheck" OFF)

# Switch to enable a more verbose output for the gmake useful for debugging)
set(CMAKE_VERBOSE_MAKEFILE OFF)

# Create json meta data for Visual Studio Code, clang-tidy, code complete and etc
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

if(ENABLE_CPPCHECK)
    message(WARNING "  === cppcheck build === ")
    set(CMAKE_CXX_CPPCHECK "cppcheck")
endif()

if(RELEASE_BUILD)
    message(WARNING "  === Release build === ")
else()
    message(WARNING "  === Debug build === ")
endif()

if (BUILD_UNITTEST)
    message(WARNING "  === Building unit-tests === ")
    enable_testing()
endif()

if(BUILD_FUZZER)
  message(WARNING "  === build for fuzzing === ")
  # set(flag "${flag} -DHAVE_FUZZER -fsanitize=address,fuzzer ")
  # set(flag "${flag} -DHAVE_FUZZER -fsanitize=address ")
  set(flag "${flag} -DHAVE_FUZZER ")
  set(ENABLE_SANITIZER "OFF")
  set(BUILD_SHARED "OFF")
endif()

if (BUILD_SHARED)
    set(BUILD_TYPE "SHARED")
else()
    set(BUILD_TYPE "STATIC")
endif()

if (BUILD_BENCHMARK)
    find_package(benchmark REQUIRED)
endif()

if(RUN_CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY clang-tidy)
endif()

# Get the current working branch from git
execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/ProjectVersion.h.in
    ${CMAKE_BINARY_DIR}/ProjectVersion.h
)

include_directories(
    ${CMAKE_BINARY_DIR}
)

# add_compile_definitions(MY_GLOBAL_VAR)

add_compile_options(
    # $<$<COMPILE_LANGUAGE:CXX>:-Weffc++>
    $<$<COMPILE_LANGUAGE:CXX>:-Wreorder>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-exceptions>
    $<$<COMPILE_LANGUAGE:C>:-Wmissing-prototypes>
    $<$<COMPILE_LANGUAGE:CXX>:-Wcast-qual>
    #-Wextra
    #-fstack-protector-strong
    #-fanalyzer
    #-Werror
    -Wpointer-arith
    -Wcast-qual
    -Wsign-compare
    -Wunused-variable
    -Wunused-value
    -Wunused-label
    -Wunused-but-set-variable
    -Wparentheses
    -Wreturn-type
    -Wno-unused-function
    -fstack-protector-all
    -Wshift-count-overflow
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough
    -fstack-protector-strong
    -fstack-clash-protection
)

if(RELEASE_BUILD)
    add_compile_options(-Wl,--no-undefined)
else()
    add_compile_options(
        -g
        -Wall
    )

    if(ENABLE_SANITIZER)
        message(WARNING "  === sanitizer is enabled === ")
        # http://clang.llvm.org/docs/AddressSanitizer.html

        add_compile_options(
            -fno-omit-frame-pointer
        )

        # add_compile_options(-fsanitize=thread)
        # add_link_options(-fsanitize=thread)

        # add_compile_options(-fsanitize=leak)
        # add_link_options(-fsanitize=leak)

        # add_compile_options(-fsanitize=address)
        # add_link_options(-fsanitize=address)

        # add_compile_options(-fsanitize=undefined)
        # add_link_options(-fsanitize=undefined)

        add_compile_options(-fsanitize=address,signed-integer-overflow)
        add_link_options(-fsanitize=address,signed-integer-overflow)

    endif()
endif()

if(ENABLE_CCACHE)
    message(WARNING "  === ccache is enabled === ")
    find_program(CCACHE_FOUND ccache)

    if(CCACHE_FOUND)
        message(WARNING "  === ccache found === ")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    endif(CCACHE_FOUND)
endif()

add_subdirectory(lib)
add_subdirectory(app)

install(DIRECTORY systemd/ DESTINATION /etc/systemd/system)

# This section generates a install package
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})

message(STATUS "  === Building Debian package === ")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER ${Package_maintainer})

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    ${CMAKE_CURRENT_SOURCE_DIR}/debian/preinst
    ${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst
    ${CMAKE_CURRENT_SOURCE_DIR}/debian/prerm
    ${CMAKE_CURRENT_SOURCE_DIR}/debian/postrm
)

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}.${GIT_COMMIT_HASH}")

# Build a CPack driven installer package
include (CPack)

