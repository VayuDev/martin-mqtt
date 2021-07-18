project(mqtt-client-cli)
set(CMAKE_CXX_STANDARD 17)

if (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fno-strict-aliasing -Wno-unused-parameter")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -fno-strict-aliasing -D_DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g -march=native -mtune=native -fno-strict-aliasing")
endif (UNIX)

if (WIN32)
    # Due to magical reasons, -Wall doesn't work under msvc currently
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Od")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /DEBUG /D_DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Od")
endif (WIN32)


file(GLOB_RECURSE HEADERS
        "${PROJECT_SOURCE_DIR}/include/*.hpp")

file(GLOB_RECURSE SOURCES
        "${PROJECT_SOURCE_DIR}/src/*.cpp")

add_executable(mqtt-client-cli ${SOURCES} ${HEADERS} ../thirdparty/cxxopts/cxxopts.h)

include_directories(../mqtt-client/include)
include_directories(include)
if (UNIX)
    target_link_libraries(mqtt-client-cli mqtt-client stdc++ m)
endif (UNIX)

if (WIN32)
    target_link_libraries(mqtt-client-cli mqtt-client)
endif (WIN32)
