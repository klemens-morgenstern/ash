
add_executable(main_test test_main.cpp tokenizer.cpp)


target_include_directories(main_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME main_test
        COMMAND $<TARGET_FILE:main_test>)
