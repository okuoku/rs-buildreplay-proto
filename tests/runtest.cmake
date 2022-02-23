if(NOT WORK)
    message(FATAL_ERROR "WORK: workdir")
endif()

if(NOT CTX) # CONTEXT
    message(FATAL_ERROR "CTX: tagname")
endif()
if(NOT IDENT)
    message(FATAL_ERROR "IDENT: tagname")
endif()

if(NOT EXEC)
    message(FATAL_ERROR "EXEC: execfilename fullpath")
endif()

if(NOT DEC)
    message(FATAL_ERROR "DEC: Decoder fullpath")
endif()

# optionals: ENVVAR ENVDATA EXPECTFAIL

# construct runner argv
set(fill)
set(args)
set(c 0)
while(1)
    math(EXPR c "${c}+1")
    message(STATUS "ARG${c}/${CMAKE_ARGC}: ${CMAKE_ARGV${c}}")
    if(c EQUAL ${CMAKE_ARGC})
        break()
    endif()
    if("${CMAKE_ARGV${c}}" STREQUAL "--")
        set(fill ON)
        continue()
    endif()
    if(fill)
        list(APPEND args ${CMAKE_ARGV${c}})
    endif()
endwhile()

set(workdir ${WORK}/${IDENT})

if(IS_DIRECTORY ${workdir})
    file(REMOVE_RECURSE ${workdir})
endif()
file(MAKE_DIRECTORY ${workdir})

set(ENV{RS_EXECLOG_LOGDIR} ${workdir})
set(ENV{RS_EXECLOG_CONTEXT} ${CTX})
set(ENV{RS_EXECLOG_IDENT} ${IDENT})
if(ENVVAR)
    set(ENV{${ENVVAR}} ${ENVDATA})
endif()

execute_process(COMMAND ${EXEC} ${args}
    RESULT_VARIABLE rr
    )

if(EXPECTFAIL)
    if(NOT rr)
        message(FATAL_ERROR "Expectfail but not")
    endif()
else()
    if(rr)
        message(FATAL_ERROR "Fail: ${rr}")
    endif()
endif()

file(GLOB bin "${workdir}/*.bin")
get_filename_component(log "${bin}" NAME_WE)
execute_process(COMMAND ${DEC} ${bin} ${workdir}/${log}.txt)

