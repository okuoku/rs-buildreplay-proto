# Pathdb

# Analysis task generation

# Tracer arm/unarm

function(rs_buildreplay_unarm pth)
    get_filename_component(pthdir ${pth} DIRECTORY)
    get_filename_component(pthname ${pth} NAME)
    set(first ${pthdir}/rs-exectrace/${pthname})
    set(second ${pthdir}/${pthname}.traced.exe)
    if(EXISTS ${first})
        message(STATUS "Unarm: ${first} => ${pth}")
        file(RENAME ${first} ${pth})
    elseif(EXISTS ${second})
        message(STATUS "Unarm: ${second} => ${pth}")
        file(RENAME ${second} ${pth})
    else()
        message(STATUS "Not found: ${second}")
    endif()
endfunction()

function(__rs_buildreplay_rearm_common tracer pth dest)
    get_filename_component(pthdir ${pth} DIRECTORY)
    get_filename_component(pthname ${pth} NAME)
    get_filename_component(tracername ${tracer} NAME)
    string(RANDOM LENGTH 13 tmpname)
    set(tmpdir ${pthdir}/tmp_${tmpname})
    file(MAKE_DIRECTORY ${tmpdir})
    if(NOT EXISTS ${dest})
        get_filename_component(destdir ${dest} DIRECTORY)
        get_filename_component(destname ${pth} NAME)
        message(STATUS "Rename: ${pth} => ${dest}")
        file(RENAME ${pth} ${dest})
    endif()
    message(STATUS "Deploy: ${pth}")
    file(MAKE_DIRECTORY ${tmpdir})
    file(COPY ${tracer} DESTINATION ${tmpdir} USE_SOURCE_PERMISSIONS)
    file(RENAME ${tmpdir}/${tracername} ${pth})
    file(REMOVE ${tmpdir})
endfunction()

function(rs_buildreplay_rearm tracer pth)
    get_filename_component(dir ${pth} DIRECTORY)
    get_filename_component(basename ${pth} NAME)

    set(destfile ${dir}/${basename}.traced.exe)
    __rs_buildreplay_rearm_common(${tracer} ${pth} ${destfile})
endfunction()

function(rs_buildreplay_rearm_alt tracer pth)
    get_filename_component(dir ${pth} DIRECTORY)
    get_filename_component(basename ${pth} NAME)

    set(destdir ${dir}/rs-exectrace)
    file(MAKE_DIRECTORY ${destdir})

    set(destfile ${destdir}/${basename})

    __rs_buildreplay_rearm_common(${tracer} ${pth} ${destfile})
endfunction()

function(rs_buildreplay_dumplog dumper pth)
    execute_process(
        COMMAND ${dumper}
        ${pth}
        ${pth}.txt)
endfunction()
