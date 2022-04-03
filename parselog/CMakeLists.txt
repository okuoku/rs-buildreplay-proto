cmake_minimum_required(VERSION 3.0)
if(NOT LOGDIR)
    message(FATAL_ERROR "LOGDIR: full path to rslog.bin directory")
endif()

project(analyzer C)

add_subdirectory(.. tracer)


set(outdir ${CMAKE_CURRENT_BINARY_DIR}/out)

file(GLOB logs CONFIGURE_DEPENDS ${LOGDIR}/*.bin)

file(MAKE_DIRECTORY ${outdir})

set(outputs)
foreach(l ${logs})
    get_filename_component(logname ${l} NAME_WE)
    set(outpath ${outdir}/${logname}.txt)
    list(APPEND outputs ${outpath})
    add_custom_command(
        COMMAND execlogger_dump
        ${l}
        ${outpath}
        OUTPUT ${outpath}
        )
endforeach()
add_custom_target(genout ALL DEPENDS ${outputs})
