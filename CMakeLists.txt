project(azlayout)
set(VERSION 1.0.0)
cmake_minimum_required(VERSION 2.8)

include(FindPkgConfig)
include(GNUInstallDirs)
include(InstallRequiredSystemLibraries)

# cairo
pkg_check_modules(CAIRO cairo)
INCLUDE_DIRECTORIES(${CAIRO_INCLUDE_DIRS})
LINK_DIRECTORIES(${CAIRO_LIBRARY_DIRS})
LINK_LIBRARIES(${CAIRO_LIBRARIES})

# freetype2
pkg_check_modules(FREETYPE2 freetype2)
INCLUDE_DIRECTORIES(${FREETYPE2_INCLUDE_DIRS})
LINK_DIRECTORIES(${FREETYPE2_LIBRARY_DIRS})
LINK_LIBRARIES(${FREETYPE2_LIBRARIES})

# harfbuzz
pkg_check_modules(HARFBUZZ harfbuzz)
INCLUDE_DIRECTORIES(${HARFBUZZ_INCLUDE_DIRS})
LINK_DIRECTORIES(${HARFBUZZ_LIBRARY_DIRS})
LINK_LIBRARIES(${HARFBUZZ_LIBRARIES})

# harfbuzz-icu
pkg_check_modules(HARFBUZZ_ICU harfbuzz-icu)
INCLUDE_DIRECTORIES(${HARFBUZZ_ICU_INCLUDE_DIRS})
LINK_DIRECTORIES(${HARFBUZZ_ICU_LIBRARY_DIRS})
LINK_LIBRARIES(${HARFBUZZ_ICU_LIBRARIES})

# fontconfig
pkg_check_modules(FONTCONFIG fontconfig)
INCLUDE_DIRECTORIES(${FONTCONFIG_INCLUDE_DIRS})
LINK_DIRECTORIES(${FONTCONFIG_LIBRARY_DIRS})
LINK_LIBRARIES(${FONTCONFIG_LIBRARIES})

set(CMAKE_C_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS "-Wall -std=c++11")

ADD_CUSTOM_TARGET(debug
                    ${CMAKE_COMMAND} 
                      -DCMAKE_BUILD_TYPE:STRING=Debug ${PROJECT_SOURCE_DIR})
ADD_CUSTOM_TARGET(release
                    ${CMAKE_COMMAND}
                      -DCMAKE_BUILD_TYPE:STRING=Release ${PROJECT_SOURCE_DIR})

set(CMAKE_C_FLAGS_DEBUG "-O0 -g -DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# Settings for clang's static-analyzer.
# Just type "make scan" in advance to "scan-build -V make".
ADD_CUSTOM_TARGET(scan
                    ${CMAKE_COMMAND}
                      -DCMAKE_BUILD_TYPE:STRING=Scan ${PROJECT_SOURCE_DIR})

if (CMAKE_BUILD_TYPE MATCHES "Scan")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -DNDEBUG") 
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -DNDEBUG") 

  # Why do I have to set full-pathes? No pkg-config file?
  set(CMAKE_C_COMPILER "/usr/share/clang/scan-build/ccc-analyzer")
  set(CMAKE_CXX_COMPILER "/usr/share/clang/scan-build/c++-analyzer")
endif(CMAKE_BUILD_TYPE MATCHES "Scan")

# Make sure to link to Harfbuzz's since it may be installed locally atm.
option(USE_LOCAL_HARFBUZZ "Use local harfbuzz instead of distro's" ON)
if(USE_LOCAL_HARFBUZZ)
  set(CMAKE_INSTALL_RPATH "${HARFBUZZ_LIBRARY_DIRS}")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif(USE_LOCAL_HARFBUZZ)

configure_file("${PROJECT_SOURCE_DIR}/src/demo.sh.in"
               demo.sh)

ADD_CUSTOM_TARGET(demo /bin/bash demo.sh)

add_executable(azlayout
               src/main.cpp)

