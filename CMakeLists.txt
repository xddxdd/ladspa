cmake_minimum_required(VERSION 3.10)

project(lantian_ladspa NONE)

set(CUSTOM_INSTALL_DIR "/usr/lib/ladspa/")
set(CUSTOM_C_FLAGS "-Wall -Werror -fPIC")
set(CUSTOM_CXX_FLAGS "${CUSTOM_C_FLAGS}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE DEBUG)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} ${CUSTOM_C_FLAGS} -O3")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} ${CUSTOM_C_FLAGS} -O0 -g")
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} ${CUSTOM_CXX_FLAGS} -O3")
SET(CMAKE_CXX_FLAGS_DEBUG  "${CMAKE_CXX_FLAGS} ${CUSTOM_CXX_FLAGS} -O0 -g")

enable_language(C)
enable_language(CXX)

file(GLOB_RECURSE CUSTOM_SOURCES src/lt_splitfreq.c)
add_library(lt_splitfreq SHARED ${CUSTOM_SOURCES})
set_target_properties(lt_splitfreq PROPERTIES PREFIX "")
set_target_properties(lt_splitfreq PROPERTIES NO_SONAME "1")
target_link_libraries(lt_splitfreq m)
install(TARGETS lt_splitfreq DESTINATION ${CUSTOM_INSTALL_DIR})

file(GLOB_RECURSE CUSTOM_SOURCES src/lt_splitfreq_dyn.c)
add_library(lt_splitfreq_dyn SHARED ${CUSTOM_SOURCES})
set_target_properties(lt_splitfreq_dyn PROPERTIES PREFIX "")
set_target_properties(lt_splitfreq_dyn PROPERTIES NO_SONAME "1")
target_link_libraries(lt_splitfreq_dyn m)
install(TARGETS lt_splitfreq_dyn DESTINATION ${CUSTOM_INSTALL_DIR})
