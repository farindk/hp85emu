
QTVERSION=5

MOC_DEFINES=
DEFINES=-fPIC
INCPATH=`pkg-config Qt5Gui Qt5Widgets --cflags`
LIBS=`pkg-config Qt5Gui Qt5Widgets --libs`

SRC=main.cc mach85.cc ioINTERNAL.cc bitmap.cc moc_mainwindow.cpp moc_crtwidget.cpp mainwindow.cc bitmap_qt.cc
HEADERS=main.hh mach85.hh ioINTERNAL.hh bitmap.h mainwindow.h crtwidget.h ui_mainwindow.h bitmap_qt.h


hp85emu: $(SRC) $(HEADERS)
	g++ -O0 -g -std=c++11 $(INCPATH) $(DEFINES) $(SRC) -o hp85emu $(LIBS)

all: hp85emu

clean:
	rm -f hp85emu
	rm -f *~
	rm -f moc_*
	rm -f ui_*

moc_%.cpp: %.h
	moc -qt=$(QTVERSION) $(MOC_DEFINES) $(INCPATH) $< -o $@

ui_%.h: %.ui
	uic -qt=$(QTVERSION) $< -o $@
