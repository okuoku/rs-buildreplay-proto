cmake_minimum_required(VERSION 3.0)
project(buildtrace-proto)

include(CTest)

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
set(testworkdir ${CMAKE_CURRENT_BINARY_DIR}/testwork)
if(IS_DIRECTORY ${testworkdir})
    file(REMOVE_RECURSE ${testworkdir})
endif()
file(MAKE_DIRECTORY ${testdir})
file(MAKE_DIRECTORY ${testworkdir})

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

add_test(NAME plain_success
    COMMAND ${testdir}/test_success${exe}
    )

add_test(NAME success
    COMMAND ${CMAKE_COMMAND}
    -DWORK=${testworkdir}
    -DDEC=$<TARGET_FILE:execlogger_dump>
    -DCTX=testing
    -DIDENT=ident
    -DEXEC=${testdir}/test_success${exe}
    -P ${CMAKE_CURRENT_LIST_DIR}/tests/runtest.cmake
    )

add_test(NAME success_args
    COMMAND ${CMAKE_COMMAND}
    -DWORK=${testworkdir}
    -DDEC=$<TARGET_FILE:execlogger_dump>
    -DCTX=testing
    -DIDENT=ident2
    -DEXEC=${testdir}/test_success${exe}
    -P ${CMAKE_CURRENT_LIST_DIR}/tests/runtest.cmake
    --
    a b c d
    )
add_test(NAME fail
    COMMAND ${CMAKE_COMMAND}
    -DWORK=${testworkdir}
    -DDEC=$<TARGET_FILE:execlogger_dump>
    -DCTX=testing
    -DIDENT=ident3
    -DEXPECTFAIL=ON
    -DEXEC=${testdir}/test_fail${exe}
    -P ${CMAKE_CURRENT_LIST_DIR}/tests/runtest.cmake
    )


