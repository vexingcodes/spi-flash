cmake_minimum_required(VERSION 3.13)

project(bedrock_flash_project
        VERSION 0.0.1
        DESCRIPTION "Bedrock Flash Chip Simulator"
        LANGUAGES CXX)

set(bedrock_flash_sources src/flash.cpp src/flash_sim.cpp)
set(bedrock_flash_headers src/flash.hpp src/flash.ipp src/flash_sim.hpp)
set(bedrock_flash_test_sources src/flash_sim_tests.cpp)

# Global options for all compilations.
set(CMAKE_CXX_STANDARD 17)
add_compile_options("-Wall" "-Wextra" "-Wpedantic" "-Werror")

# Unit tests are only allowed for Debug builds. Code coverage only applies if unit tests are enabled.
if (CMAKE_BUILD_TYPE MATCHES Debug)
  option(UNIT_TESTS "Enable unit tests." OFF)
  if (UNIT_TESTS)
    find_package(Catch2 REQUIRED)
    enable_testing()
    include (CTest)
    include (Catch)
    option(CODE_COVERAGE "Enable code coverage collection for unit tests." OFF)
    if (CODE_COVERAGE)
      add_compile_options("-O0" "-g" "--coverage")
    endif ()
  endif ()
endif ()

# Install the header files for development against the library.
install(FILES ${bedrock_flash_headers} DESTINATION include/bedrock/flash)

# Build the sources once for both the shared and static libraries.
add_library(bedrock_flash_objects OBJECT ${bedrock_flash_sources})
set_target_properties(bedrock_flash_objects PROPERTIES POSITION_INDEPENDENT_CODE 1)

# Build a shared library, if requested.
option(BUILD_SHARED "Build shared object bedrock_flash.so" ON)
if (BUILD_SHARED)
  add_library(bedrock_flash_shared SHARED $<TARGET_OBJECTS:bedrock_flash_objects>)
  set_target_properties(bedrock_flash_shared PROPERTIES OUTPUT_NAME bedrock_flash)
  install(TARGETS bedrock_flash_shared DESTINATION lib)
  if (CODE_COVERAGE)
    target_link_libraries(bedrock_flash_shared gcov)
  endif ()
endif ()

# Build a static library, if requested.
option(BUILD_STATIC "Build static library bedrock_flash.a" ON)
if (BUILD_STATIC)
  add_library(bedrock_flash_static STATIC $<TARGET_OBJECTS:bedrock_flash_objects>)
  set_target_properties(bedrock_flash_static PROPERTIES OUTPUT_NAME bedrock_flash)
  install(TARGETS bedrock_flash_static DESTINATION lib)
  if (CODE_COVERAGE)
    target_link_options(bedrock_flash_static INTERFACE --coverage)
  endif ()
endif ()

# Don't allow the user to build nothing.
if (NOT BUILD_SHARED AND NOT BUILD_STATIC)
  message(FATAL_ERROR "Must select one or more of BUILD_SHARED or BUILD_STATIC")
endif ()

# Build the test binary, if requested.
if (UNIT_TESTS)
  add_executable(bedrock_flash_tests ${bedrock_flash_test_sources})
  catch_discover_tests(bedrock_flash_tests)

  # Prefer linking with the shared library. If only the static library is being built, then link against that. If both
  # library types are being built, provide an option to link with the static library, but default to the shared library.
  if (BUILD_SHARED AND NOT BUILD_STATIC)
    set(TEST_LINK_STATIC OFF CACHE INTERNAL "Link the unit test binary with the static library?")
  elseif (BUILD_STATIC AND NOT BUILD_SHARED)
    set(TEST_LINK_STATIC ON CACHE INTERNAL "Link the unit test binary with the static library?")
  else ()
    option(TEST_LINK_STATIC "Link the unit test binary with the static library?" OFF)
  endif ()

  if (TEST_LINK_STATIC)
    target_link_libraries(bedrock_flash_tests bedrock_flash_static)
  else ()
    target_link_libraries(bedrock_flash_tests bedrock_flash_shared)
  endif ()
endif ()

# Build documentation, if requested.
option(DOCUMENTATION "Create HTML documentation using Doxygen." OFF)
if (DOCUMENTATION)
  find_package(Doxygen)
  if (NOT DOXYGEN_FOUND)
    message (FATAL_ERROR "Doxygen is needed to build the documentation.")
  endif ()
  doxygen_add_docs(docs ${bedrock_flash_headers})
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html DESTINATION share/doc/bedrock-flash)
endif ()

# Create a target to format the code, if requested.
option(FORMAT "Use clang-format to force coding style." OFF)
if (FORMAT)
  # Determine the proper executable to use for clang-format.
  if (NOT CLANGFORMAT)
    set(CLANGFORMAT clang-format)
  endif ()
  if (NOT EXISTS ${CLANGFORMAT})
    find_program(clangformat_tmp ${CLANGFORMAT})
    if (clangformat_tmp)
      set(CLANGFORMAT ${clangformat_tmp})
      unset(clangformat_tmp)
    else ()
      message(FATAL_ERROR "ClangFormat: ${CLANGFORMAT} not found! Aborting")
    endif ()
  endif ()

  add_custom_target(format
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    COMMAND ${CLANGFORMAT}
                            -style=file
                            -i
                            ${bedrock_flash_sources} ${bedrock_flash_headers} ${bedrock_flash_test_sources})
endif ()

# Add a target for generating code coverage HTML reports if code coverage is enabled.
if (CODE_COVERAGE)
  # Determine the proper executable to use for lcov.
  if (NOT LCOV)
      set(LCOV lcov)
  endif ()
  if (NOT EXISTS ${LCOV})
    find_program(lcov_tmp ${LCOV})
    if (lcov_tmp)
      set(LCOV ${lcov_tmp})
      unset(lcov_tmp)
    else ()
      message(FATAL_ERROR "lcov: ${LCOV} not found! Aborting")
    endif ()
  endif ()

  # Determine the proper executable to use for genhtml.
  if (NOT GENHTML)
      set(GENHTML genhtml)
  endif ()
  if (NOT EXISTS ${GENHTML})
    find_program(genhtml_tmp ${GENHTML})
    if (genhtml_tmp)
      set(GENHTML ${genhtml_tmp})
      unset(genhtml_tmp)
    else ()
      message(FATAL_ERROR "genhtml: ${GENHTML} not found! Aborting")
    endif ()
  endif ()

  add_custom_target(coverage
                    COMMAND ${LCOV} --capture --directory . --output-file coverage.info
                    COMMAND ${LCOV} -e coverage.info '${CMAKE_CURRENT_SOURCE_DIR}/src/*' -o coverage.info
                    COMMAND ${GENHTML} coverage.info --output-directory coverage)
endif ()
