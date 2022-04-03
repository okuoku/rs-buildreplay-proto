if(NOT TGT)
    message(FATAL_ERROR "TGT: full path to traced executable.")
endif()

if(NOT MODE)
    message(FATAL_ERROR "MODE: ARM | UNARM")
endif()

# Tracer is required even on unarm, to get correct traced path
if(NOT TRACER)
    message(FATAL_ERROR "TRACER: full path to tracer executable.")
endif()


# Calc paths

get_filename_component(dir ${TGT} DIRECTORY)
get_filename_component(basename ${TGT} NAME_WE)
get_filename_component(tracer_name ${TRACER} NAME)

# FIXME: Support non-Win32 
set(traced ${dir}/${basename}.traced.exe)

function(copy_tracer)
    # Copy tracer to random directory first, then rename
    string(RANDOM LENGTH 13 tmpname)
    set(tmpdir ${dir}/tmp_${tmpname})
    file(MAKE_DIRECTORY ${tmpdir})
    file(COPY ${TRACER} DESTINATION ${tmpdir})
    file(RENAME ${tmpdir}/${tracer_name} ${TGT})
    file(REMOVE ${tmpdir})
endfunction()

if(MODE STREQUAL ARM)
    if(EXISTS ${traced})
        file(REMOVE ${TGT})
        copy_tracer()
        message(STATUS "Re-armed: ${TGT} => ${traced}")
    else()
        file(RENAME ${TGT} ${traced})
        copy_tracer()
        message(STATUS "Armed: ${TGT} => ${traced}")
    endif()
elseif(MODE STREQUAL UNARM)
    if(EXISTS ${traced})
        file(REMOVE ${TGT})
        file(RENAME ${traced} ${TGT})
        message(STATUS "Unarmed: ${traced} => ${TGT}")
    else()
        message(STATUS "Not armed: ${TGT}")
    endif()
else()
    message(FATAL_ERROR "Unknown mode: ${MODE}")
endif()
