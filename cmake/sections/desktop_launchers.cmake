# Tests that open native windows which take focus, capture or activation fall
# in two groups, by what they need from the desktop:
# - `foreground`: a desktop of their own is enough. On a development machine
#   they run on a private desktop of the same window station (projects
#   tools/run_on_desktop.py), where nothing is ever foreground, so they never
#   take the user's foreground and may run in parallel, without a lock.
# - `input_desktop`: they need the desktop the user sees. The UI Automation
#   and MSAA clients (off the input desktop every UI Automation pattern call
#   waits 2 s inside UIAutomationCore) and the compositor probe (DWM does not
#   composite another desktop). They carry the desktop_foreground resource
#   lock, which serializes them within one ctest run; across ctest runs
#   (several workers on one machine) a launcher holds a machine-wide lock.
# Each group has a launcher, a CMake list run in front of the test executable
# (CMake 3.29 or later, for TEST_LAUNCHER). The cache value wins; when it is
# empty, the environment variable of the same name is read at every
# configure. For example:
#   GUI_FORMS_FOREGROUND_LAUNCHER="python;-B;D:/malkuth/projects/tools/run_on_desktop.py;--"
#   GUI_FORMS_INPUT_DESKTOP_LAUNCHER="python;-B;D:/malkuth/projects/tools/foreground_lock.py;--"
# CI leaves both empty: each runner has a desktop of its own.
set(GUI_FORMS_FOREGROUND_LAUNCHER "" CACHE STRING
    "Command that runs a foreground test on a private desktop (empty: the environment's, else none)")
set(GUI_FORMS_INPUT_DESKTOP_LAUNCHER "" CACHE STRING
    "Command that runs an input-desktop test under a machine-wide lock (empty: the environment's, else none)")
# Sets the TEST_LAUNCHER of test `test_name` (whose executable target has the
# same name) from cache variable `variable`, or from the environment variable
# of that name when the cache value is empty.
function(gui_forms_desktop_launcher test_name variable)
    set(launcher "${${variable}}")
    if(NOT launcher)
        set(launcher "$ENV{${variable}}")
    endif()
    if(NOT launcher)
        return()
    endif()
    if(CMAKE_VERSION VERSION_LESS 3.29)
        message(FATAL_ERROR "${variable} needs CMake 3.29 or later (TEST_LAUNCHER)")
    endif()
    set_property(TARGET ${test_name} PROPERTY TEST_LAUNCHER ${launcher})
endfunction()
function(gui_forms_foreground_test test_name)
    set_property(TEST ${test_name} APPEND PROPERTY LABELS foreground)
    gui_forms_desktop_launcher(${test_name} GUI_FORMS_FOREGROUND_LAUNCHER)
endfunction()
function(gui_forms_input_desktop_test test_name)
    set_property(TEST ${test_name} APPEND PROPERTY LABELS input_desktop)
    set_property(TEST ${test_name} APPEND PROPERTY RESOURCE_LOCK desktop_foreground)
    gui_forms_desktop_launcher(${test_name} GUI_FORMS_INPUT_DESKTOP_LAUNCHER)
endfunction()
