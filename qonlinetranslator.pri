QT += network multimedia

HEADERS += $$PWD/src/qonlinetranslator.h \
    $$PWD/src/qonlinetts.h \
    $$PWD/src/qexample.h \
    $$PWD/src/qoption.h
    $$PWD/src/bingvoicecatalog.h
    $$PWD/src/bingvoicecatalog_data.h

SOURCES += $$PWD/src/qonlinetranslator.cpp \
    $$PWD/src/qonlinetts.cpp \
    $$PWD/src/qexample.cpp \
    $$PWD/src/qoption.cpp
    $$PWD/src/bingvoicecatalog.cpp

INCLUDEPATH += $$PWD/src

CONFIG += c++1z
