cmake_minimum_required(VERSION 3.16)

project(TestFileFilterProxyModel LANGUAGES CXX)

enable_testing()

# Wir fügen das Präfix "../" vor jede Datei in der Liste ein
set(PREVIEW_SOURCES "")
foreach(FILE ${PREVIEW_COMMON})
    list(APPEND PREVIEW_SOURCES "../../${FILE}")
endforeach()

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets LinguistTools Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Sql Test)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(TestFileFilterProxyModel tst_filefilterproxymodel.cpp)

# Erzwinge den Konsolen-Modus (entfernt die Suche nach WinMain)
set_target_properties(TestFileFilterProxyModel PROPERTIES
    WIN32_EXECUTABLE FALSE
)

add_test(NAME TestFileFilterProxyModel COMMAND TestFileFilterProxyModel)

target_link_libraries(TestFileFilterProxyModel PRIVATE
    CommonObjects
    Qt${QT_VERSION_MAJOR}::Sql
    Qt6::Test
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(TestFileFilterProxyModel)
endif()
