#target = Raspberry
# target = SDL20
# This target below is for some cross compilation from linux to windows
# it works, but windows gives 2 channels instead of the requested 1, creating bad sound when run in wine, and no sound at all in native windows 11 !
# so consider it experimental for now !
# target = x86_64-w64-mingw32
target=i686-w64-mingw32
CXX = g++
MD = mkdir
# comment out following line to see compiler invocation
SILENT=1

ifeq ($(target), Raspberry)
  options = -O -D_LINUX -D RGB655 -D SDL12  -c
  gincludes =  -I/usr/include/SDL
  libs =  -lSDL
  comp = gcc $(options) $(gincludes)
endif
ifeq ($(target), SDL20)
	#	debug options (make sure the ldflags line is uncommented if you use this)
	options =   -g -fsanitize=address -D _LINUX -D RGB655 -D SDL20 -c
# optimized build options (comment out ldflags below to remove -fsanitize=address!!!)
	#options =   -O2 -D _LINUX -D RGB655 -D SDL20 -c
	ldflags := -g -fsanitize=address
options += -Isdl_sound -Iimgui
gincludes := \
               $(shell pkg-config --cflags sdl2)
libs :=  $(shell sdl2-config --libs)
# -std=c++11 is required for imgui
comp := g++ -std=c++11 $(options) $(gincludes)
compc := gcc $(options) $(gincludes)
endif
ifeq ($(target),x86_64-w64-mingw32)
	WIN=1
endif
ifeq ($(target),i686-w64-mingw32)
	WIN=1
endif
ifdef WIN # windows version
	options =  -O2 -D _LINUX -D RGB655 -D SDL20 -c -DMINGW
  gincludes = -Isdl_sound -Iimgui -Ifnmatch \
               $(shell $(target)-pkg-config --cflags sdl2)
  libs =  $(shell $(target)-pkg-config --libs sdl2)
  comp = $(target)-g++ -std=c++11 $(options) $(gincludes)
  compc = $(target)-gcc $(options) $(gincludes)
  CXX = $(target)-g++
  WINDRES=$(target)-windres
endif
ifdef SILENT
	comp:=@${comp}
	compc:=@$(compc)
	CXX:=@${CXX}
	MD:=@$(MD)
endif

OBJS=	DSA.o       data.o         Code11f52.o  Graphics.o  Magic.o          \
        CSBCode.o   Timer.o        Character.o  system.o    SmartDiscard.o   \
        VBL.o       Mouse.o        Recording.o  LinCSBUI.o  CSBlinux.o       \
        Code51a4.o  CSBUI.o        MoveObject.o Hint.o      Monster.o        \
        Code17818.o Viewport.o     AsciiDump.o  Menu.o      Chaos.o          \
        SaveGame.o  Translation.o  Bitmaps.o    md5C.o      utility.o        \
        WinScreen.o Code13ea4.o    Codea59a.o   RC4.o       Attack.o         \
        Sound.o     Code1f9e6.o    Code390e.o   Code222ea.o Statistics.o     \
        SoundMixer.o

OBJDIR = obj
OBJDIRS = $(OBJDIR) \
		  $(OBJDIR)/sdl_sound \
		  $(OBJDIR)/imgui \
		  $(OBJDIR)/fnmatch

OBJS := $(OBJS:%=$(OBJDIR)/%)

SDL_SOUND = SDL_sound.o SDL_sound_mp3.o SDL_sound_raw.o SDL_sound_wav.o
SDL_SOUND := $(SDL_SOUND:%=$(OBJDIR)/sdl_sound/%)
OBJS += $(SDL_SOUND)

IMGUI = imgui.o imgui_draw.o imgui_impl_sdl2.o imgui_tables.o imgui_widgets.o imgui_impl_sdlrenderer2.o imguifilebrowser.o
IMGUI := $(IMGUI:%=$(OBJDIR)/imgui/%)
OBJS += $(IMGUI)

ifdef WIN
	OBJS += obj/fnmatch/fnmatch.o
endif

all: maketree message csb

$(OBJDIRS):
	$(MD) -p $@

# include optionally the dependancies for the include files...
-include $(OBJS:%.o=%.P)

ifdef WIN
OBJS += $(OBJDIR)/csb.res

$(OBJDIR)/csb.res: CSBwin.rc lsb.ico
	$(WINDRES) -O coff -o $(OBJDIR)/csb.res -i CSBwin.rc

lsb.ico: lsb.png
	# requires imagemagick!
	convert lsb.png lsb.ico

endif

csb: $(OBJS)
	@echo Linking $@
	$(CXX)     \
        $(OBJS) $(libs) \
	    $(ldflags) \
	    -o csb

maketree: $(OBJDIRS)

message:
	@rm -f obj/CSBlinux.o # force recompilation for every build!
	@echo -n "Building csb, "
ifneq "$(findstring -O, $(options))" ""
	@echo -n optimized
else
	@echo -n debug
endif
	@echo " version"

clean:
	rm -rf obj csb

# This tricky rule generates dependancies information in .P files while compiling files
# it avoids to maintain a list of include files in the makefile
$(OBJDIR)/%.o: %.cpp
	@echo Compiling $<...
	$(comp) -MD -c $< -o $@
	@cp $(OBJDIR)/$*.d $(OBJDIR)/$*.P; \
            sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
                -e '/^$$/ d' -e 's/$$/ :/' < $(OBJDIR)/$*.d >> $(OBJDIR)/$*.P; \
            rm -f $(OBJDIR)/$*.d

$(OBJDIR)/%.o: %.c
	@echo Compiling $<...
	$(compc) -MD -c $< -o $@
	@cp $(OBJDIR)/$*.d $(OBJDIR)/$*.P; \
            sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
                -e '/^$$/ d' -e 's/$$/ :/' < $(OBJDIR)/$*.d >> $(OBJDIR)/$*.P; \
            rm -f $(OBJDIR)/$*.d
