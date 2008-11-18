include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../engine)

find_package(KDE4 REQUIRED)

set(archivereadertest_SRCS
    archivereadertest.cpp
)

set(archivecreatetest_SRCS
    archivecreatetest.cpp
)

set(roundtriptest_SRCS
    roundtriptest.cpp
)

set(protostest_SRCS
    protostest.cpp
)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

kde4_add_executable(archivereadertest TEST ${archivereadertest_SRCS})
target_link_libraries(archivereadertest 
    arkiveengine ${KDE4_KDECORE_LIBS} ${QT_QTCORE_LIBRARY})

kde4_add_executable(archivecreatetest TEST ${archivecreatetest_SRCS})
target_link_libraries(archivecreatetest
    arkiveengine ${KDE4_KDECORE_LIBS} ${QT_QTCORE_LIBRARY})

# kde4_add_executable(roundtriptest TEST ${roundtriptest_SRCS})
# target_link_libraries(archivereadertest
#     arkiveengine ${KDE4_KDECORE_LIB} ${QT_QTCORE_LIBRARY})

kde4_add_executable(protostest TEST ${protostest_SRCS})
target_link_libraries(protostest
    arkiveengine ${QT_QTCORE_LIBRARY})
