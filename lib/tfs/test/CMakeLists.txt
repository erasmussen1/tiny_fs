project(test_tfs CXX)

add_executable(${PROJECT_NAME}
    test_tfs.cpp
)

target_link_libraries(${PROJECT_NAME}
    LINK_PUBLIC
        tfs
	    -lgtest
	    -lgtest_main
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# install(DIRECTORY inc DESTINATION ${CMAKE_INSTALL_PREFIX}/include )
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib )

