project(tfs)

add_library(${PROJECT_NAME}
    ${BUILD_TYPE}
        src/tfs.c
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        inc
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        src
)

if(BUILD_UNITTEST)
    add_subdirectory(test)
endif()

install(DIRECTORY inc/ DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

