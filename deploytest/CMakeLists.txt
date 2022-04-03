cmake_minimum_required(VERSION 3.0)
project(deploytest C)

add_subdirectory(.. tracer)

set(tool_names
    arm-none-eabihf-gcc
    # arm-none-eabihf-cc # FIXME: Cannot handle Cygwin symlink
    arm-none-eabihf-g++
    arm-none-eabihf-c++
    arm-none-eabihf-as
    arm-none-eabihf-ar
    arm-none-eabihf-ld
    arm-none-eabihf-objcopy
    arm-none-eabihf-objdump
    )

add_custom_target(arm)
add_custom_target(unarm)

foreach(t ${tool_names})

    # FIXME: MS build prompt cannot detect Cygwin executables..?
    #find_program(prog_${t} ${t} REQUIRED)
    set(prog_${t}
        f:/armnone-m4f/crosstool-ng/prefix/bin/${t}.exe)

    add_custom_target(arm_${t}
        COMMAND ${CMAKE_COMMAND}
        -DTGT=${prog_${t}} -DTRACER=$<TARGET_FILE:execlogger>
        -DMODE=ARM
        -P ${CMAKE_CURRENT_LIST_DIR}/tracearmunarm.cmake
        DEPENDS execlogger)

    add_custom_target(unarm_${t}
        COMMAND ${CMAKE_COMMAND}
        -DTGT=${prog_${t}} -DTRACER=$<TARGET_FILE:execlogger>
        -DMODE=UNARM
        -P ${CMAKE_CURRENT_LIST_DIR}/tracearmunarm.cmake
        )

    add_dependencies(unarm unarm_${t})
    add_dependencies(arm arm_${t})
endforeach()


