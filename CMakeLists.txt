cmake_minimum_required(VERSION 3.0)
project(buildtrace-proto)

if(WIN32)
    add_executable(execlogger
        execlogger_win32.c)
else()
    add_executable(execlogger
        execlogger_posix.c)
endif()

if(WIN32 OR CYGWIN)
    set(exe ".exe")
else()
    set(exe "")
endif()

add_executable(execlogger_dump
    execlogger_dump.c)

add_executable(test_fail
    tests/test_fail.c)

add_executable(test_success
    tests/test_success.c)

# Deploy testdir
set(testdir ${CMAKE_CURRENT_BINARY_DIR}/testdir)
file(MAKE_DIRECTORY ${testdir})

add_custom_command(TARGET execlogger
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy 
    ${CMAKE_CURRENT_BINARY_DIR}/execlogger${exe}
    ${testdir}/test_success${exe}
    COMMAND ${CMAKE_COMMAND} -E copy 
    ${CMAKE_CURRENT_BINARY_DIR}/execlogger${exe}
    ${testdir}/test_fail${exe}
    )

add_custom_command(TARGET test_success
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_BINARY_DIR}/test_success${exe}
    ${testdir}/test_success.traced${exe})

add_custom_command(TARGET test_fail
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_BINARY_DIR}/test_fail${exe}
    ${testdir}/test_fail.traced${exe})

