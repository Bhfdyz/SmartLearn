QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    aichatpage.cpp \
    connectmanager.cpp \
    knowledgedialog.cpp \
    learningpathpage.cpp \
    logindialog.cpp \
    main.cpp \
    mainwindow.cpp \
    registerdialog.cpp \
    stageitem.cpp

HEADERS += \
    aichatpage.h \
    config.h \
    connectmanager.h \
    knowledgedialog.h \
    learningpathpage.h \
    logindialog.h \
    mainwindow.h \
    registerdialog.h \
    stageitem.h

FORMS += \
    knowledgedialog.ui \
    logindialog.ui \
    mainwindow.ui \
    registerdialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res.qrc
