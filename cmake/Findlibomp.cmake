# Locate the library
find_path(libomp_INCLUDE omp.h PATHS /opt/homebrew/Cellar/libomp/19.1.7/include)
find_library(libomp_LIBRARY NAMES omp PATHS /opt/homebrew/Cellar/libomp/19.1.7/lib)

# Provide the results to the calling project
if(libomp_INCLUDE AND libomp_LIBRARY)
    set(libomp_FOUND TRUE)
else()
    set(libomp_FOUND FALSE)
endif()

mark_as_advanced(libomp_INCLUDE libomp_LIBRARY)