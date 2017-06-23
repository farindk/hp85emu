
QTVERSION=5
MOC_DEFINES=

CXX=g++
CPPFLAGS=-fPIC `pkg-config Qt5Gui Qt5Widgets --cflags`
CXXFLAGS=-std=c++11 -g -O0
LIBS=`pkg-config Qt5Gui Qt5Widgets --libs`

OBJS=main.o mach85.o ioINTERNAL.o bitmap.o moc_mainwindow.o moc_crtwidget.o mainwindow.o bitmap_qt.o tape.o
HEADERS=main.hh mach85.hh ioINTERNAL.hh bitmap.h mainwindow.h crtwidget.h ui_mainwindow.h bitmap_qt.h tape.h


hp85emu: $(OBJS) $(HEADERS)
	g++ $(OBJS) -o hp85emu $(LIBS)

all: hp85emu

clean:
	rm -f hp85emu
	rm -f *~
	rm -f moc_*
	rm -f ui_*
	rm -f *.o

moc_%.cc: %.h
	moc -qt=$(QTVERSION) $(MOC_DEFINES) $(INCPATH) $< -o $@

ui_%.h: %.ui
	uic -qt=$(QTVERSION) $< -o $@

%.o: %.cc
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $< -o $@
