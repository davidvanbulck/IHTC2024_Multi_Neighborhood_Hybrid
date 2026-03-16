cmake_minimum_required (VERSION 3.5)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
project(ihtc)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "-DCLIQUE -DNDEBUG -DMINIMALPRINT")
file(GLOB_RECURSE SOURCES source/*.cpp source/*.cc source/*.c)
file(GLOB_RECURSE HEADER_FILES source/*.hpp source/*.h)

add_subdirectory(dlib)


foreach(HEADER_FILE ${HEADER_FILES})
    get_filename_component(HEADER_DIRECTORY ${HEADER_FILE} DIRECTORY)
    list(APPEND INCLUDE_DIRECTORIES ${HEADER_DIRECTORY})
endforeach()

list(REMOVE_DUPLICATES INCLUDE_DIRECTORIES)

# try to find gurobi
# uses FindGUROBI.cmake in cmake dir
find_package(GUROBI REQUIRED)
if(GUROBI_FOUND)
    message(STATUS "Gurobi found")
    message(STATUS "Gurobi include dirs: ${GUROBI_INCLUDE_DIR}")
    message(STATUS "Gurobi libraries: ${GUROBI_LIBRARIES}")
else()
    message(STATUS "Gurobi not found")
endif()

#find boost
find_package(Boost REQUIRED)
if(Boost_FOUND)
    message(STATUS "Boost found")
endif()

# find libomp (only on macos)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_package(libomp REQUIRED)
    if(libomp_FOUND)
        message(STATUS "libomp found")
        message(STATUS "libomp include dirs: ${libomp_INCLUDE}")
        message(STATUS "libomp libraries: ${libomp_LIBRARY}")
    else()
        message(STATUS "libomp not found")
    endif()
endif()
add_executable(ihtc ${SOURCES})

target_include_directories(
    ihtc 
    PUBLIC
    ${INCLUDE_DIRECTORIES}
    ${GUROBI_INCLUDE_DIR}
    ${libomp_INCLUDE}
    ${Boost_INCLUDE_DIRS}
    dlib
)

target_link_libraries(ihtc PUBLIC ${GUROBI_LIBRARIES} ${libomp_LIBRARY} ${Boost_LIBRARY_DIRS})
