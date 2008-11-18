include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

MACRO(ARKIVE_UNIT_TESTS)
       FOREACH(_testname ${ARGN})
               kde4_add_unit_test(${_testname} NOGUI ${_testname}.cpp)
               target_link_libraries(${_testname} arkiveengine ${QT_QTTEST_LIBRARY})
       ENDFOREACH(_testname)
ENDMACRO(ARKIVE_UNIT_TESTS)

ARKIVE_UNIT_TESTS(
    RingBufferTest
)
