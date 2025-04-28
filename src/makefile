#target = Raspberry
target = Ubuntu_SDL20
# This target below is for some cross compilation from linux to windows
# it works, but windows gives 2 channels instead of the requested 1, creating bad sound when run in wine, and no sound at all in native windows 11 !
# so consider it experimental for now !
#target = x86_64-w64-mingw32
CXX = g++
# comment out following line to see compiler invocation
SILENT=1

ifeq ($(target), Raspberry)
  options = -O -D_LINUX -D RGB655 -D SDL12  -c
  gincludes =  -I/usr/include/SDL
  libs =  -lSDL
  comp = gcc $(options) $(gincludes)
endif
ifeq ($(target), Ubuntu_SDL20)
	#	debug options (make sure the ldflags line is uncommented if you use this)
	#options =   -g -fsanitize=address -D _LINUX -D RGB655 -D SDL20 -c
# optimized build options (comment out ldflags below to remove -fsanitize=address!!!)
	options =   -O2 -D _LINUX -D RGB655 -D SDL20 -c
# ldflags := -g -fsanitize=address
gincludes := \
               $(shell pkg-config --cflags sdl2)
libs :=  $(shell sdl2-config --libs)
comp := g++ $(options) $(gincludes)
ifdef SILENT
	comp:=@${comp}
	CXX:=@${CXX}
endif
endif
ifeq ($(target),x86_64-w64-mingw32)
	options =   -O2 -D _LINUX -D RGB655 -D SDL20 -c -DMINGW
  gincludes =  \
               $(shell $(target)-pkg-config --cflags sdl2)
  libs =  $(shell $(target)-pkg-config --libs sdl2)
  comp = $(target)-g++ $(options) $(gincludes)
  CXX = $(target)-g++
endif

all: CSB

OBJS=	DSA.o       data.o         Code11f52.o  Graphics.o  Magic.o          \
        CSBCode.o   Timer.o        Character.o  system.o    SmartDiscard.o   \
        VBL.o       Mouse.o        Recording.o  LinCSBUI.o  CSBlinux.o       \
        Code51a4.o  CSBUI.o        MoveObject.o Hint.o      Monster.o        \
        Code17818.o Viewport.o     AsciiDump.o  Menu.o      Chaos.o          \
        SaveGame.o  Translation.o  Bitmaps.o    md5C.o      utility.o        \
        WinScreen.o Code13ea4.o    Codea59a.o   RC4.o       Attack.o         \
        Sound.o     Code1f9e6.o    Code390e.o   Code222ea.o Statistics.o     \
        SoundMixer.o

# include optionally the dependancies for the include files...
-include $(OBJS:%.o=%.P)

CSB: $(OBJS)
	@echo Linking CSB
	$(CXX)     \
        $(OBJS) $(libs) \
	    $(ldflags) \
	    -o ./CSB/CSB

clean:
	rm -f *.o CSB/CSB *.P

# This tricky rule generates dependancies information in .P files while compiling files
# it avoids to maintain a list of include files in the makefile
%.o: %.cpp
	@echo Compiling $<...
	$(comp) -MD -c $< -o $@
	@cp $*.d $*.P; \
            sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
                -e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.P; \
            rm -f $*.d

