QT       += core gui charts

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    comandiprincipali.cpp \
    de10silicon.cpp \
    mainwindow.cpp \
    qcustomplot.cpp \
    slowcontrol.cpp

HEADERS += \
    comandiprincipali.h \
    de10silicon.h \
    mainwindow.h \
    qcustomplot.h \
    slowcontrol.h

FORMS += \
    comandiprincipali.ui \
    mainwindow.ui \
    slowcontrol.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    out.dat
