cmake_minimum_required(VERSION 3.16)

project(OpenOffice VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---- Qt6 ----
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Gui)

# ---- sources ----
set(SOURCES
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/core/ooxmlmodel.h
    src/core/ooxmlmodel.cpp
    src/core/ziputil.h
    src/core/ziputil.cpp
    src/io/docxreader.h
    src/io/docxreader.cpp
    src/io/docxwriter.h
    src/io/docxwriter.cpp
    src/ui/editorwidget.h
    src/ui/editorwidget.cpp
    src/ui/aipanel.h
    src/ui/aipanel.cpp
    src/ai/aiprovider.h
    src/ai/openaiprovider.h
    src/ai/openaiprovider.cpp
    src/ai/aimanager.h
    src/ai/aimanager.cpp
)

add_executable(OpenOffice ${SOURCES})

target_include_directories(OpenOffice PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(OpenOffice PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Gui
)

# ---- optional zip self-test console tool ----
option(BUILD_ZIP_TEST "Build the ZipUtil self-test console tool" ON)
if(BUILD_ZIP_TEST)
    add_executable(zip_test tests/zip_test.cpp src/core/ziputil.h src/core/ziputil.cpp)
    target_include_directories(zip_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(zip_test PRIVATE Qt6::Core)

    add_executable(oo_doc_test tests/doctest.cpp
        src/core/ooxmlmodel.h src/core/ooxmlmodel.cpp
        src/core/ziputil.h src/core/ziputil.cpp
        src/io/docxreader.h src/io/docxreader.cpp
        src/io/docxwriter.h src/io/docxwriter.cpp)
    target_include_directories(oo_doc_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(oo_doc_test PRIVATE Qt6::Core Qt6::Gui)
endif()
