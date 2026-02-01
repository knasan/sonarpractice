cmake_minimum_required(VERSION 3.16)

project(SonarPractice VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets LinguistTools Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Sql Test)

set(TS_FILES SonarPractice_de_DE.ts)

set(PROJECT_SOURCES
    main.cpp
)

set(PREVIEW_COMMON
    basepage.cpp              filefilterproxymodel.h
    filterpage.cpp     librarypage.h
    mappingpage.h
    songeditdialog.cpp  uihelper.cpp
    basepage.h
    filemanager.cpp
    filterpage.h

    setupwizard.cpp
    songeditdialog.h    uihelper.h
    databasemanager.cpp       filemanager.h
    fnv1a.h            mainwindow.cpp       setupwizard.h   welcomepage.cpp
    databasemanager.h         filescanner.h           importprocessor.h  mainwindow.h     reviewpage.cpp      sonarlessonpage.cpp       welcomepage.h
    filefilterproxymodel.cpp  fileutils.h             librarypage.cpp    mappingpage.cpp  reviewpage.h        sonarlessonpage.h
    CACHE INTERNAL "Common files for main project and tests"
)

# Enable CommonObjects for testing project
# Erstelle die Library
add_library(CommonObjects OBJECT ${PREVIEW_COMMON}
    sonarstructs.h
    reviewstruct.h
    filescanner.cpp

    fileselectiondialog.h fileselectiondialog.cpp
    README_de.md
    README.md

)

# Aktiviere Qt für diese Library (Löst den "QObject not found" Fehler)
target_link_libraries(CommonObjects PUBLIC
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt6::Test Qt6::Widgets
)

# target_include_directories(CommonObjects PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}) [cite: 2]

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(SonarPractice
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        lupdate.bat
        styleGuide.md
    )

target_link_libraries(SonarPractice PRIVATE
    CommonObjects
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
)

# Define target properties for Android with Qt 6 as:
#    set_property(TARGET SonarPractice APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

# qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
qt_add_translations(SonarPractice
    TS_FILES ${TS_FILES}
    RESOURCE_PREFIX "/i18n"
)
else()
    if(ANDROID)
        add_library(SonarPractice SHARED
            ${PROJECT_SOURCES}
        )
    # Define properties for Android with Qt 5 after find_package() calls as:
    #    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
else()
    add_executable(SonarPractice
        ${PROJECT_SOURCES}
    )
endif()

qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()


target_include_directories(CommonObjects PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.SonarPractice)
endif()
set_target_properties(SonarPractice PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    AUTOMOC ON
)

include(GNUInstallDirs)
install(TARGETS SonarPractice
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(SonarPractice)
endif()
add_subdirectory(SonarPracticeTests)
