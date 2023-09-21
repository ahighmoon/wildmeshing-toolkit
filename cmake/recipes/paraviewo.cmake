
if(TARGET paraviewo::paraviewo)
    return()
endif()

message(STATUS "Third-party (external): creating target 'paraviewo::paraviewo'")
option(PARAVIEWO_BUILD_TESTS "Build Tests" OFF)
set(PARAVIEWO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
include(FetchContent)
FetchContent_Declare(
    paraviewo
    GIT_REPOSITORY https://github.com/polyfem/paraviewo.git
    GIT_TAG 67d6092312202233df3233b7dc156180e7ac81ce
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(paraviewo)

set_target_properties(paraviewo PROPERTIES FOLDER third_party)
set_target_properties(fmt PROPERTIES FOLDER third_party)
set_target_properties(tinyxml2 PROPERTIES FOLDER third_party)

set_target_properties(hdf5-static PROPERTIES FOLDER third_party)
set_target_properties(hdf5_hl-static PROPERTIES FOLDER third_party)