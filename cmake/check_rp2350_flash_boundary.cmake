if(NOT DEFINED MI23_ELF)
    message(FATAL_ERROR "MI23_ELF is required")
endif()
if(NOT DEFINED MI23_OBJDUMP)
    message(FATAL_ERROR "MI23_OBJDUMP is required")
endif()
if(NOT DEFINED MI23_FLASH_BOUNDARY)
    message(FATAL_ERROR "MI23_FLASH_BOUNDARY is required")
endif()

if(NOT EXISTS "${MI23_ELF}")
    message(FATAL_ERROR "RP2350 ELF not found: ${MI23_ELF}")
endif()

execute_process(
        COMMAND "${MI23_OBJDUMP}" -h "${MI23_ELF}"
        RESULT_VARIABLE objdump_result
        OUTPUT_VARIABLE objdump_output
        ERROR_VARIABLE objdump_error
)
if(NOT objdump_result EQUAL 0)
    message(FATAL_ERROR "Failed to inspect RP2350 ELF sections: ${objdump_error}")
endif()

set(xip_base 0x10000000)
set(max_flash_end 0)

string(REPLACE "\n" ";" objdump_lines "${objdump_output}")
foreach(line IN LISTS objdump_lines)
    if(line MATCHES "^[ \t]*[0-9]+[ \t]+[^ \t]+[ \t]+([0-9A-Fa-f]+)[ \t]+([0-9A-Fa-f]+)[ \t]+([0-9A-Fa-f]+)")
        set(section_size_hex "${CMAKE_MATCH_1}")
        set(section_lma_hex "${CMAKE_MATCH_3}")
        math(EXPR section_size "0x${section_size_hex}")
        math(EXPR section_lma "0x${section_lma_hex}")
        math(EXPR flash_start "${xip_base}")
        math(EXPR flash_limit "${xip_base} + ${MI23_FLASH_BOUNDARY}")
        if(section_size GREATER 0 AND
           NOT section_lma LESS flash_start AND
           section_lma LESS flash_limit)
            math(EXPR section_end "${section_lma} - ${xip_base} + ${section_size}")
            if(section_end GREATER max_flash_end)
                set(max_flash_end "${section_end}")
            endif()
        endif()
    endif()
endforeach()

if(max_flash_end GREATER MI23_FLASH_BOUNDARY)
    message(FATAL_ERROR
            "RP2350 firmware image ends at flash offset ${max_flash_end}, "
            "which overlaps reserved settings/filesystem storage starting at ${MI23_FLASH_BOUNDARY}.")
endif()

message(STATUS
        "RP2350 firmware flash end ${max_flash_end} is below reserved storage boundary ${MI23_FLASH_BOUNDARY}")
