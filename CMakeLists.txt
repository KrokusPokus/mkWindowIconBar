cmake_minimum_required(VERSION 3.22)
project(mkWindowSwitcher VERSION 0.1.0 LANGUAGES C CXX) # C: the generated Wayland protocol code

set(QT_MIN_VERSION "6.8.0")
set(KF6_MIN_VERSION "6.10.0")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQmlModule)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Gui Qml Quick Concurrent WaylandClient)
find_package(Qt6GuiPrivate ${QT_MIN_VERSION} REQUIRED NO_MODULE)       # QNativeInterface::Private::QWaylandWindow
find_package(Plasma REQUIRED)                                          # plasma_add_applet(), Plasma::Applet
find_package(PlasmaWaylandProtocols 1.10 REQUIRED)                     # plasma-window-management.xml

set(APPLET_ID io.github.krokuspokus.windowswitcher)

plasma_add_applet(${APPLET_ID}
    QML_SOURCES
        qml/main.qml
        qml/WindowButton.qml
    CPP_SOURCES
        src/windowmodel.cpp
        src/windowmodel.h
    GENERATE_APPLET_CLASS
)

qt6_generate_wayland_protocol_client_sources(${APPLET_ID}
    PRIVATE_CODE
    FILES ${PLASMA_WAYLAND_PROTOCOLS_DIR}/plasma-window-management.xml
)

# The generated QML type registration includes the header by its bare name
target_include_directories(${APPLET_ID} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(${APPLET_ID} PRIVATE
    Qt::Core
    Qt::Gui
    Qt::GuiPrivate
    Qt::Qml
    Qt::Quick
    Qt::Concurrent
    Qt::WaylandClient
)
