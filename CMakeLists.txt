cmake_minimum_required(VERSION 3.20)
project(claimledger VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)

set(CLAIMLEDGER_SOURCES
  src/sparse.cpp
  src/graph.cpp
  src/laplacian.cpp
  src/lanczos.cpp
  src/embedding.cpp
  src/clustering.cpp
  src/bridges.cpp
  src/csv.cpp
  src/json.cpp
  src/io.cpp
  src/ledger.cpp
  src/timeline.cpp
)

add_library(claimledger_core STATIC ${CLAIMLEDGER_SOURCES})
target_include_directories(claimledger_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_features(claimledger_core PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(claimledger_core PRIVATE /W4)
else()
  target_compile_options(claimledger_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(claimledger src/main.cpp)
target_link_libraries(claimledger PRIVATE claimledger_core)

# --- tests -----------------------------------------------------------------
include(CTest)
option(CLAIMLEDGER_FETCH_GTEST "Fetch GoogleTest" ON)

if(BUILD_TESTING)
  set(_have_gtest FALSE)
  if(CLAIMLEDGER_FETCH_GTEST)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    set(_have_gtest TRUE)
  endif()

  if(_have_gtest)
    add_executable(claimledger_tests
      tests/test_graph.cpp
      tests/test_laplacian.cpp
      tests/test_lanczos.cpp
      tests/test_bridges.cpp
      tests/test_ledger.cpp
      tests/test_csv.cpp
      tests/test_timeline.cpp
      tests/test_arxiv_citations.cpp
    )
    target_link_libraries(claimledger_tests PRIVATE claimledger_core GTest::gtest_main)
    target_compile_definitions(claimledger_tests PRIVATE
      CLAIMLEDGER_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/data/fixtures")
    include(GoogleTest)
    gtest_discover_tests(claimledger_tests
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      DISCOVERY_TIMEOUT 30)
  endif()
endif()

# --- COBOL (optional) ------------------------------------------------------
find_program(COBC_EXECUTABLE cobc)
if(COBC_EXECUTABLE)
  set(COBOL_BIN_DIR ${CMAKE_BINARY_DIR}/cobol)
  file(MAKE_DIRECTORY ${COBOL_BIN_DIR})
  add_custom_target(cobol-ledger
    COMMAND ${COBC_EXECUTABLE} -free -I ${CMAKE_SOURCE_DIR}/cobol/copy -x
            -o ${COBOL_BIN_DIR}/post-citation
            ${CMAKE_SOURCE_DIR}/cobol/src/POST-CITATION.cbl
    COMMAND ${COBC_EXECUTABLE} -free -I ${CMAKE_SOURCE_DIR}/cobol/copy -x
            -o ${COBOL_BIN_DIR}/trial-balance
            ${CMAKE_SOURCE_DIR}/cobol/src/TRIAL-BALANCE.cbl
    COMMAND ${COBC_EXECUTABLE} -free -I ${CMAKE_SOURCE_DIR}/cobol/copy -x
            -o ${COBOL_BIN_DIR}/report
            ${CMAKE_SOURCE_DIR}/cobol/src/REPORT.cbl
    COMMENT "Compiling GnuCOBOL ledger programs"
    VERBATIM)
  message(STATUS "cobc found: ${COBC_EXECUTABLE} (target cobol-ledger available)")
else()
  message(STATUS "cobc not found — COBOL sources ship with a Python verifier (scripts/verify_ledger.py)")
endif()

install(TARGETS claimledger RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
