TEMPLATE = vcapp

# ---- Link QtUiTools properly (handles Debug/Release on Windows) ----
win32:CONFIG(release, debug|release): LIBS += -lQtUiTools
win32:CONFIG(debug, debug|release):   LIBS += -lQtUiToolsd
QT += gui 
CONFIG += warn_on
HEADERS += PropertyLink.h
SOURCES += app.cpp PropertyLink.cpp
