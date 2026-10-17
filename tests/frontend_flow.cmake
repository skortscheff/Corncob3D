# Runs the menus unattended (scripted keys, a canned mission result) and checks what they saved:
# a pilot is created, a theater opened and flown, the mission scored and put in the high score list.
file(REMOVE_RECURSE ${SAVE})
execute_process(
    COMMAND ${EXE} --skip-intro --no-music --save-dir ${SAVE} --fake-mission 14
            --ui-keys "d,c,text:Ace,Enter,Enter,Esc,c,o,Enter,Enter,f,Enter,Enter,Enter,Enter,Enter,Enter,Enter,Enter,Enter,Enter"
    RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "the front end exited with ${rc}")
endif()
file(READ ${SAVE}/career.dat career)
foreach(want "pilot|1|Ace|0" "theater|1|DUSK" "high|1|Ace|0|97500|DUSK")
    string(FIND "${career}" "${want}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "career.dat does not contain ${want}:\n${career}")
    endif()
endforeach()
if(NOT EXISTS ${SAVE}/theater1.cct)
    message(FATAL_ERROR "the theater universe was not saved")
endif()
file(REMOVE_RECURSE ${SAVE})
message(STATUS "frontend flow ok")
