QT       += core gui websockets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    broker.cpp \
    main.cpp \
    mainwindow.cpp \
    sslserver.cpp

HEADERS += \
    broker.h \
    mainwindow.h \
    sslserver.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

#Copy additional files to build dir
CONFIG += file_copies
COPIES += sslCertAndKey icons iniFiles
sslCertAndKey.files = $$files(localhost.*)
icons.files = $$files(icons/*)
sslCertAndKey.path = $$OUT_PWD
icons.path = $$OUT_PWD/icons
iniFiles.files = $$files(fastecu-broker.ini)
iniFiles.path = $$OUT_PWD

DISTFILES += \
    fastecu-broker.ini
