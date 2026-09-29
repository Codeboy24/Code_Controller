cmake_minimum_required(VERSION 3.16)
project(ControllerCoder VERSION 0.01 LANGUAGES C)

include(CTest)

add_executable(ControllerCoder WIN32
    Main.c
    controller.c
    snippets.c
    wheel.c
)

target_compile_features(ControllerCoder PRIVATE c_std_11)
target_compile_definitions(ControllerCoder PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0601
)
target_link_libraries(ControllerCoder PRIVATE user32 gdi32 winmm)

if(MINGW)
    target_link_libraries(ControllerCoder PRIVATE m)
    target_link_options(ControllerCoder PRIVATE -municode)
endif()

if(BUILD_TESTING)
    add_executable(ControllerCoderTests
        tests.c
        controller.c
        snippets.c
        wheel.c
    )
    target_compile_features(ControllerCoderTests PRIVATE c_std_11)
    target_compile_definitions(ControllerCoderTests PRIVATE
        UNICODE
        _UNICODE
        _WIN32_WINNT=0x0601
    )
    target_link_libraries(ControllerCoderTests PRIVATE user32 gdi32 winmm)
    if(MINGW)
        target_link_libraries(ControllerCoderTests PRIVATE m)
    endif()
    add_test(NAME ControllerCoderTests COMMAND ControllerCoderTests)
endif()
