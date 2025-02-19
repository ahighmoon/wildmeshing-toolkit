if (TARGET probabilistic_quadrics::probabilistic_quadrics)
    return()
endif()

message(STATUS "Third-party: creating target 'probabilistic_quadrics::probabilistic_quadrics'")

FetchContent_Declare(
    probabilistic_quadrics
    GIT_REPOSITORY https://github.com/Philip-Trettner/probabilistic-quadrics.git
    GIT_TAG        4920ade95eeaa65f61a70fffea3c389a023977a7
)

FetchContent_MakeAvailable(probabilistic_quadrics)

add_library(probabilistic_quadrics INTERFACE)
add_library(probabilistic_quadrics::probabilistic_quadrics ALIAS probabilistic_quadrics)

target_include_directories(probabilistic_quadrics
    INTERFACE
    ${probabilistic_quadrics_SOURCE_DIR})

target_compile_features(probabilistic_quadrics INTERFACE cxx_std_17)
