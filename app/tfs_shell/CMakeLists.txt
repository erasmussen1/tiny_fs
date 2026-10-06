
project(tfs_shell)

add_executable(${PROJECT_NAME}
    tfs_shell.cpp
)

target_link_libraries(${PROJECT_NAME}
    LINK_PUBLIC
        tfs
)

install(TARGETS ${PROJECT_NAME}
    DESTINATION
        ${CMAKE_INSTALL_PREFIX}/bin
)

