# Locate the library
find_path(GUROBI_INCLUDE_DIR gurobi_c++.h PATHS /Library/gurobi1100/macos_universal2/include /apps/gent/RHEL9/zen2-ib/software/Gurobi/12.0.0-GCCcore-13.2.0/include)
find_library(GUROBI_LIBRARY_STATIC NAMES gurobi_c++ PATHS /Library/gurobi1100/macos_universal2/lib /apps/gent/RHEL9/zen2-ib/software/Gurobi/12.0.0-GCCcore-13.2.0/lib)
find_library(GUROBI_LIBRARY_SHARED NAMES gurobi120 PATHS /Library/gurobi1100/macos_universal2/lib /apps/gent/RHEL9/zen2-ib/software/Gurobi/12.0.0-GCCcore-13.2.0/lib)

set(GUROBI_LIBRARIES ${GUROBI_LIBRARY_STATIC} ${GUROBI_LIBRARY_SHARED})

# Provide the results to the calling project
if(GUROBI_INCLUDE_DIR AND GUROBI_LIBRARIES)
    set(GUROBI_FOUND TRUE)
else()
    set(GUROBI_FOUND FALSE)
endif()

mark_as_advanced(GUROBI_INCLUDE_DIR GUROBI_LIBRARIES)
