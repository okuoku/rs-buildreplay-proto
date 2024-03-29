cmake_minimum_required(VERSION 3.0)
if(NOT INDIR)
    message(FATAL_ERROR "INDIR: full path to converted .txt directory")
endif()
if(NOT OUT)
    message(FATAL_ERROR "OUTDIR: full path to converted .cmake file")
endif()

file(GLOB logs ${INDIR}/*.txt)
list(LENGTH logs len)
message(STATUS "Reading ${len} files...")
string(ASCII 1 _ESC)

set(cnt 0)
set(ids)
foreach(f ${logs})
    math(EXPR cnt "${cnt}+1")
    math(EXPR mod "${cnt}%100")
    if(mod EQUAL 0)
        message(STATUS "Processing ${cnt}/${len} ...")
    endif()
    # Make sure the file do not contain semicolons
    file(READ ${f} dat)
    string(REGEX REPLACE ";" "${_ESC}" dat2 "${dat}")
    if(NOT dat STREQUAL dat2)
        message(FATAL_ERROR "Invalid character in: ${f}")
    endif()
    # Escape end-of-line backslash
    string(REPLACE "\\\n" "${_ESC}\n" dat "${dat}") 
    # Split file into list-of-lines
    string(REGEX REPLACE "\n" ";" lines "${dat}")
    set(argfs)
    set(argf)
    set(argfid 0)
    set(argfstack)
    set(workdir)
    set(ident)
    set(pident)
    set(result)
    foreach(l ${lines})
        if(${l} MATCHES "(....):(.*)")
            set(tag ${CMAKE_MATCH_1})
            set(content "${CMAKE_MATCH_2}")
            if(argf) 
                # in ARGD/ARGE region
                if(tag STREQUAL ARGD)
                    list(APPEND argfreg ${content})
                elseif(tag STREQUAL ARGE)
                    list(APPEND argfstack ${argfid})
                    set(${ident}_args_${argfid} ${argfreg})
                    set(${ident}_args_${argfid}_name ${argf})
                    math(EXPR argfid "${argfid}+1")
                    set(argfreg)
                    set(argf)
                endif()
            else()
                if(tag STREQUAL IDNP)
                    if(pident)
                        message(FATAL_ERROR "Multiple IDNP: ${f}")
                    else()
                        set(pident ${content})
                    endif()
                elseif(tag STREQUAL IDNT)
                    if(ident)
                        message(FATAL_ERROR "Multiple IDNT: ${f}")
                    else()
                        set(ident ${content})
                    endif()
                elseif(tag STREQUAL WRKD)
                    if(workdir)
                        message(FATAL_ERROR "Multiple WRKD: ${f}")
                    else()
                        set(workdir ${content})
                    endif()
                elseif(tag STREQUAL PRES)
                    if(result)
                        message(FATAL_ERROR "Multiple PRES: ${f}")
                    else()
                        set(result ${content})
                    endif()
                elseif(tag STREQUAL ARGB)
                    if(NOT ident)
                        message(FATAL_ERROR "No IDNT before ARGB: ${f}")
                    endif()
                    set(argfreg)
                    if(content)
                        set(argf ${content})
                    else()
                        set(argf __command_line__)
                    endif()
                else()
                    message(STATUS "Unrecognised tag(ignored): ${tag} in ${f}")
                endif()
            endif()
        else()
            message(STATUS "INVALID: [${l}]")
            message(FATAL_ERROR "Invalid string in: ${f}")
        endif()
    endforeach()

    set(${ident}_result ${result})
    set(${ident}_workdir ${workdir})
    set(${ident}_pident ${pident})
    set(${ident}_argfs ${argfid})
    set(${ident}_argstack ${argfstack})
    list(APPEND ids ${ident})
endforeach()

message(STATUS "Writing to: ${OUT}")

file(WRITE ${OUT} "# Autogenerated, do not edit\n\n")

set(cnt 0)
foreach(id ${ids})
    math(EXPR cnt "${cnt}+1")
    math(EXPR mod "${cnt}%100")
    if(mod EQUAL 0)
        message(STATUS "Writing ${cnt}/${len} ...")
    endif()
    file(APPEND ${OUT} "\nset(${id}_result ${${id}_result})\n")
    # Escape workdir(for Win32 toolchain)
    set(workdir ${${id}_workdir})
    string(REPLACE "\\" "\\\\" workdir ${workdir})
    string(REPLACE "\"" "\\\"" workdir ${workdir})
    file(APPEND ${OUT} "set(${id}_workdir \"${workdir}\")\n")
    file(APPEND ${OUT} "set(${id}_pident ${${id}_pident})\n")
    file(APPEND ${OUT} "set(${id}_argfs ${${id}_argfs})\n")
    foreach(loc ${${ident}_argstack})
        # Escape response file name(for Win32 toolchain)
        set(name ${${id}_args_${loc}_name})
        string(REPLACE "\\" "\\\\" name ${name})
        string(REPLACE "\"" "\\\"" name ${name})
        file(APPEND ${OUT} "set(${id}_args_${loc}_name \"${name}\")\n")
        file(APPEND ${OUT} "set(${id}_args_${loc}\n")
        foreach(l ${${id}_args_${loc}})
            # Escape string
            string(REPLACE "\\" "\\\\" l ${l})
            string(REPLACE "\"" "\\\"" l ${l})
            string(REPLACE "${_ESC}" "\\\\" l ${l})
            file(APPEND ${OUT} "\t\"${l}\"\n")
        endforeach()
        file(APPEND ${OUT} ")\n")
    endforeach()
endforeach()

file(APPEND ${OUT} "\nset(rslogs ${ids})\n")

message(STATUS "Done.")
