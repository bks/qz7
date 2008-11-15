include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libqz7/include ${CMAKE_CURRENT_BINARY_DIR})

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

MACRO(QZ7_TESTS)
       FOREACH(_testname ${ARGN})
               automoc4_add_executable(${_testname} ${_testname}.cpp)
               target_link_libraries(${_testname} qz7 ${QT_QTCORE_LIBRARY})
       ENDFOREACH(_testname)
ENDMACRO(QZ7_TESTS)

QZ7_TESTS(
    GzipTest
)