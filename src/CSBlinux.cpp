// CSBlinux.cpp : Linux-specific file instead of windows-specific CSBwin.cpp.
// Note: CSBTypes.h needs to be included after stdafx.h
#include "stdafx.h"
#include "UI.h"
#include "resource.h"
#include <stdio.h>

#include "CSBTypes.h"

#include "Dispatch.h"
#include "Objects.h"
#include "CSB.h"
#include "Data.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "imguifilebrowser.h"

imgui_addons::ImGuiFileBrowser file_dialog; // As a class member or globally
bool imgui_active = false;
char *opened_file;

#define FONT_SIZE 14
#define DY (FONT_SIZE+6)

#define APPTITLE  "CSBwin"
#define APPVERSION  "15.7"
#define APPVERMINOR "v0" /* linux-only update*/

//extern gboolean noDirtyRect;
//extern SDL_Rect DirtyRect;
SDL_sem *sem;

static bool cursorIsShowing;
extern bool RecordMenuOption;
extern bool DMRulesDesignOption;
extern bool RecordDesignOption;
extern bool extendedPortraits;
bool fullscreenActive;
extern bool simpleEncipher; //not used in Linux
extern unsigned char *encipheredDataFile; //not used in Linux

static SDL_Event evert;
bool sdlQuitPending = false;
bool timerInQueue = false;

int absMouseX;  // screen resolution; you must divide by screenSize
int absMouseY;  //    to get position on ST screen.

#if defined SDL12
int screenWidth, screenHeight;
#endif


#if defined SDL20
SDL_Window *sdlWindow = NULL;
SDL_Texture *sdlTexture = NULL;
SDL_Renderer *sdlRenderer = NULL;
SDL_Rect desktop;
int SDL_VIDEOEXPOSE = 0;
#endif


#ifdef MAEMO_NOKIA_770
# include "hildon-widgets/hildon-program.h"
# include "hildon-widgets/hildon-window.h"

  HildonProgram *program = NULL;
  HildonWindow *hildonmainwindow = NULL;
#endif//MAEMO_NOKIA_770

void PostQuitMessage( int a ) {
    SDL_Event event;
//    FILE *f;
//    f = fopen("debug","a");
//    fprintf(f,"PostQuitMessage(0x%x)\n", a);
//    fclose(f);
    while (SDL_PollEvent(&event) != 0){};
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
    sdlQuitPending = true;
//    SDL_Quit ();
#ifdef USE_OLD_GTK
    gtk_exit (0);
#endif //USE_OLD_GTK
//    exit(0);
}

#define IDC_Timer 1

/*
static void DumpEvents(void)
{
  FILE *f;
  SDL_Event ev;
  f = fopen("debug","a");
  fprintf(f, "%s\n", SDL_GetError());
  while (SDL_PollEvent(&ev) != 0)
  {
    if (ev.type = SDL_USEREVENT)
    {
      fprintf(f, "SDL_USEREVENT type = %d\n", ev.user.code);
    }
    else
    {
      fprintf(f, "Event type= %d\n", ev.type);
    };
  };
  fclose(f);
}
*/

bool videoExposeWaiting = false;

static uint32_t __Timer_Callback(uint32_t interval, void *param)
{
  size_t code;
  code = (size_t)(param);
  uint32_t ticks = SDL_GetTicks();
  if (code == IDC_Timer)
  {
      SDL_SemWait(sem);
    if(timerInQueue)
    {
      // We will add an IDC_Timer only when there is nothing
      // else to do.
	SDL_SemPost(sem);
      return interval;
    }
    else
    {
      // The queue is empty.  We can add an IDC_Timer and
      // (perhaps) an IDC_VIDEOEXPOSE.
      SDL_Event ev;
      if (videoExposeWaiting)
      {
        ev.type = SDL_USEREVENT;
        ev.user.code = IDC_VIDEOEXPOSE;
        if (SDL_PushEvent(&ev) < 0)
        {
//           DumpEvents();
          die(0x66a9);
        };
        videoExposeWaiting = false;
      }
      {
        ev.type = SDL_USEREVENT;
        ((SDL_UserEvent*) &ev)->code = IDC_Timer;
        if (SDL_PushEvent(&ev) < 0)
        {
          char line[80];
          UI_MessageBox(SDL_GetError(), "PushEvent Failed",MESSAGE_OK);
          die(0x66aa);
        };
        timerInQueue = true;
        videoExposeWaiting = false;
      }
      SDL_SemPost(sem);
      return interval;
    }
  }
  else
  {
    PostQuitMessage(0xbab3);
  };
  return interval; // to avoid warning about not returning anything
}


static void PushEvent(void *param)
{
  SDL_Event ev;
  {
    size_t code;
    code = (size_t)(param);
    if (code == IDC_VIDEOEXPOSE)
    {
      if (SDL_PeepEvents(&ev,1,SDL_PEEKEVENT,
                     SDL_FIRSTEVENT,SDL_LASTEVENT) >0)
      {
        // We will add a IDC_VIDEOEXPOSE only when there is
        // nothing else to do.
        videoExposeWaiting = true;
        return;
      };
    };
  };
  ev.type = SDL_USEREVENT;
  ((SDL_UserEvent*) &ev)->code = (size_t)(param);
  if (SDL_PushEvent(&ev) < 0)
  {
    char line[80];
    sprintf(line," code = %d", (size_t)(param));
    UI_MessageBox(SDL_GetError(), "PushEvent Failed",MESSAGE_OK);
    UI_Die(0x66ab);
  };
  return;
}

int WindowWidth = 320*4;
int WindowHeight = 200*4 + DY;
float st_X = 320.0 / WindowWidth;
float st_Y = 200.0 / (WindowHeight - 20);
static CSB_UI_MESSAGE csbMessage;

static void __resize_screen( ui32 w, i32 h ) {
#if 1
    if (w < 640 || h < 400) {
	WindowWidth = 640;
	WindowHeight = 400;
    } else {
	WindowWidth = w;
	WindowHeight = h;
    }
    st_X = 320.0/WindowWidth;
    st_Y = 200.0/WindowHeight;
    SDL_SetWindowSize(sdlWindow,WindowWidth,WindowHeight);
    csbMessage.type=UIM_REDRAW_ENTIRE_SCREEN;
    if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
    {
	PostQuitMessage(0x24);
    }
#else
  SDL_Event ev;
#if defined SDL12
  //ev.type = SDL_VIDEORESIZE;
  //((SDL_ResizeEvent*)&ev) -> w = w;
  //((SDL_ResizeEvent*)&ev) -> h = h;
#elif defined SDL20
  ev.type = IDC_VIDEORESIZE;
  ev.user.data1 = (void*)((size_t)w);
  ev.user.data2 = (void*)((size_t)h);
#else
  xxxError
#endif
  SDL_PushEvent(&ev);
#endif
}
/*
* There IS a videoexpose event in linux, but apparently SDL
* wants to handle those for itself... so we cannot use 'invalidate'
* the obvious way. UI_Invalidate just marks a request to
* update the screen... LIN_Invalidate is called from vbl and pretends
* to be the OS that grants the update.
*/

bool GetVideoRectangle(i32, RECT *);
static bool pending_update = true;

void UI_Invalidate(bool erase/*=false*/) {
  pending_update = true;
}
void LIN_Invalidate()
{// *screen, x, y, w, h
  if( pending_update ) {
    PushEvent((void*)(IDC_VIDEOEXPOSE) );
  }
}




#define MAX_LOADSTRING 100



i32 trace=-1;
#ifdef _DYN_WINSIZE
//i32 screenSize=2;
#else
//i32 screenSize=2;
#endif

extern bool BeginRecordOK;
extern bool ItemsRemain;
extern bool PlayfileIsOpen(void);
extern bool RecordfileIsOpen(void);
extern bool TimerTraceActive;
extern bool AttackTraceActive;
extern bool AITraceActive;
extern bool PlaybackCommandOption;
extern unsigned char *encipheredDataFile;// Oh?

ui32 TImER=0;
extern i32 NoSpeedLimit;
extern i32 GameMode;
extern i32 MostRecentlyAdjustedSkills[2];
extern i32 LatestSkillValues[2];
extern XLATETYPE latestScanType;
extern XLATETYPE latestCharType;
extern i32 latestCharp1;
extern i32 latestScanp1;
extern i32 latestScanp2;
extern i32 latestScanXlate;
extern i32 latestCharXlate;
const char *helpMessage = "CSBlinux will need the orginal datafiles "
      "for either Chaos Strikes Back,\n"
      "or Dungeon Master in your current working directory.\n"
      "If the files are located somewhere else try\n --help for some usefull comandlinde arguments.\n"
      "\nMake sure that you have these files\n"
      "(and that they are spelled with lower case)\n"
      "   dungeon.dat\n"
      "   hcsb.dat\n"
      "   hcsb.hct\n"
      "   mini.dat\n"
      "   graphics.dat\n"
      "   config.linux";

/* This is a bad place for these id's */

void MTRACE(const char *msg)
{
  if (trace < 0) return;
  FILE *f = GETFILE(trace);
  fprintf(f, "%s", msg);
  fflush(f);
}

void UI_GetCursorPos(i32 *_x, i32 *_y)
{
  *_x = X_TO_CSB(absMouseX,screenSize);
  *_y = Y_TO_CSB(absMouseY,screenSize);
}



char szCSBVersion[] = "CSB for Windows/Linux Version " __DATE__;
int WindowX = 0;
int WindowY = 0;
bool fullscreenRequested = false;


i32 line = 0;

// Global Variables:
SDL_Surface *WND = NULL; //defined in UI.h
SDL_TimerID timer;

#ifdef _DYN_WINSIZE
HermesHandle from_palette;
HermesHandle to_palette;
HermesHandle converter;
HermesFormat* from_format;
#else
SDL_Surface *SCRAP = NULL;
#endif

void done_imgui() {
    // Cleanup
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

/* Free the global allocated data and finish the main loop */
void cbAppDestroy(void)
{
  /* Close SDL (prevents the ticks to continue running) */
  //printf("\nQuitting...\n");
    Cleanup(true);
    done_imgui();
    doneCaches();
    SDL_RemoveTimer(timer);
    SDL_DestroySemaphore(sem);
    sem = NULL;
  SDL_Quit ();
#ifdef USE_OLD_GTK
  /* End with the GTK*/
  gtk_exit (0);
#endif //USE_OLD_GTK
    exit(0);
}

void g_log(const char *, int, const char *, ...)
{
    die(0x3512);
}

extern bool resetWhatToDo,resetgamesetup,resetprisondoor;

void Process_SDL_MOUSEMOTION(
           bool& cursorIsShowing)
{
  MTRACE("SDL_MOUSEMOTION\n");
  SDL_MouseMotionEvent *e = (SDL_MouseMotionEvent*) &evert;
  int x, y;
  bool warp = false;
  x = e->x * st_X;
  y = (e->y - DY) * st_Y;
  if (y < 0) y = 0;
#if 0
  // I don't see the reason of this for now
  if (x >= 316*screenSize) {x = 316*screenSize-1; warp=true;};
  if (y >= 196*screenSize) {y = 196*screenSize-1; warp=true;}
  if (x <    1*screenSize) {x =   1*screenSize;   warp=true;}
  if (y <    1*screenSize) {y =   1*screenSize;   warp=true;}
#endif
  if (warp)
  {
#ifdef SDL12
    SDL_WarpMouse(x, y);
#elif defined SDL20
//    SDL_WarpMouseGlobal(x, y);
#else
    xxError
#endif
    return;
  };
  absMouseX = x;
  absMouseY = y;
  i32 st_mouseX = X_TO_CSB(absMouseX,screenSize);
  i32 st_mouseY = Y_TO_CSB(absMouseY,screenSize);
  if (resetgamesetup) return;
  if (GameMode == 1)
  {
    if (cursorIsShowing && !imgui_active)
    {
      cursorIsShowing = false;
      SDL_ShowCursor(SDL_DISABLE);
    };
  }
  else
  {
    if (!cursorIsShowing)
    {
      cursorIsShowing = true;
      SDL_ShowCursor(SDL_ENABLE);
    };
  };
//  {
//    FILE *f;
//    static int prevX, prevY;
//    if ((e->x != prevX) || (e->y != prevY))
//    {
//      f = fopen("/run/shm/debug","a");
//      fprintf(f, "Movement e->x=%d(%d), e->y=%d(%d); set mouseX=%d mouseY=%d showing=%d\n",
//                  e->x, e->xrel, e->y, e->yrel, absMouseX, absMouseY, cursorIsShowing);
//      fclose(f);
//    };
//  };
}

void Process_ecode_IDC_Normal(void)
{
  MTRACE("IDC_Normal\n");
  __resize_screen( 320, 240 );
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_NORMAL;
  csbMessage.p2 = 2; //2-(screenSize==1); //new value
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(1);
  };
}

void Process_ecode_IDC_Timer(void)
{
  MTRACE("IDC_Timer\n");
  if(GameMode != 1)
  {
    LIN_Invalidate();
  }
  SDL_SemWait(sem);
  timerInQueue = false;
  SDL_SemPost(sem);
       //One-at-a-time, please
       //The problem was that under certain circumstances too
       //many timer events get queued and the SDL queue gets
       //full and keystrokes get discarded!
  csbMessage.type=UIM_TIMER;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(2);
  };
}

void Process_ecode_IDC_VIDEOEXPOSE(void)
{
  /*
   * BEGIN the evil VIDEOEXPOSE
   */
  MTRACE("IDC_VIDEOEXPOSE\n");
  //printf("IDC_VIDEOEXPOSE\n");
  line++;
  line = line%10;
  //        memset( SCR, line, 40 );
  //        SDL_BlitSurface(SCR, NULL, WND, NULL );
  switch(line)
  {
    case 0:
      //        g_print("vblint = %u",VBLInterruptCount);
      break;
    case 1:
      //        g_print("chkvbl = %u",CheckVBLCount);
      break;
    case 2:
    //        g_print("STblt = %u",STBLTCount);
      break;
    case 3:
    //  g_print("Time = %u",GameTime);
      break;
    case 4:
    //  g_print(
    //         "Skill[%d]=0x%08x      ",
    //          MostRecentlyAdjustedSkills[0],
    //          LatestSkillValues[0]);
      break;
    case 5:
    //  g_print(
    //          "Skill[%d]=0x%08x      ",
    //          MostRecentlyAdjustedSkills[1],
    //          LatestSkillValues[1]);
      break;
    case 6:
      switch (latestCharType)
      {
        case TYPEIGNORED:
    //      g_print( "%04x key --> Ignored                         ", latestCharp1);
          break;
        case TYPEKEY:
    //      g_print( "%04x key --> Translated %08x", latestCharp1, latestCharXlate);
          break;
      };
      break;
    case 7:
      switch (latestScanType)
      {
        case TYPESCAN:
        case TYPEIGNORED:
    //      g_print( "%08x mscan --> Ignored                        ",latestScanp1);
          break;
        case TYPEMSCANL:
    //      g_print( "%08x mscan --> Translated to %08x L",latestScanp1,latestScanXlate);
          break;
        case TYPEMSCANR:
    //      g_print( "%08x mscan --> Translated to %08x R",latestScanp1,latestScanXlate);
          break;
        default:
    //      g_print("                                              ");
          break;
      };
      break;
    case 8:
      switch (latestScanType)
      {
        case TYPEMSCANL:
        case TYPEIGNORED:
        case TYPEMSCANR:
    //      g_print( "%08x scan --> Ignored                        ",latestScanp2);
          break;
        case TYPESCAN:
    //      g_print( "%08x scan --> Translated to %08x", latestScanp2,latestScanXlate);
          break;
        default:
    //      g_print("                                              ");
          break;
      };
      break;
    case 9:
    //  g_print("Total Moves = %d",totalMoveCount);
      break;
  };
  //TextOut(hdc,325,25+15*line,msg,strlen(msg));
  //        g_warning(msg);
  /*
   * This message should be sent when the screen has been erased?
   *
   */
  /*
    csbMessage.type=UIM_REDRAW_ENTIRE_SCREEN;
    if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
    {
      PostQuitMessage(0);
      break;
    };
  */

  /*
   * Finally, write to the screen!
   */
  csbMessage.type=UIM_PAINT;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(3);
  };
}

void Process_ecode_IDC_Double(void)
{
  MTRACE("IDC_Double\n");
  __resize_screen( 320*2, 200*2);
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_DOUBLE;
  csbMessage.p2 = 2; //2-(screenSize==2); //new value
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(4);
  };
}

void Process_ecode_IDC_Triple(void)
{
  MTRACE("IDC_Triple\n");
  __resize_screen( 320*3, 200*3 );
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_TRIPLE;
  csbMessage.p2 = 2; //2-(screenSize==3); //new value
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(5);
  };
}

void Process_ecode_IDC_Quadruple(void)
{
  MTRACE("IDC_Quadruple\n");
  __resize_screen( 320*4, 200*4 );
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_QUADRUPLE;
  csbMessage.p2 = 2; //2-(screenSize==4); //new value
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(6);
  };
}

void Process_ecode_IDC_Quintuple(void)
{
  MTRACE("IDC_Quintuple\n");
  __resize_screen( 320*5, 200*5 );
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_QUINTUPLE;
  csbMessage.p2 = 2; //2-(screenSize==4); //new value
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(7);
  };
}

void Process_ecode_IDC_Sextuple(void)
{
  MTRACE("IDC_Sextuple\n");
  __resize_screen( 320*6, 200*6 );
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_SEXTUPLE;
  csbMessage.p2 = 2; //2-(screenSize==4); //new value
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(8);
  };
}

void Process_ecode_IDC_QuickPlay(void)
{
  MTRACE("IDC_QuickPlay\n");
  if (!PlayfileIsOpen()) return;
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_QUICKPLAY;
  csbMessage.p2 = (NoSpeedLimit!=0) ? 0 : 1;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(9);
  };
}

void Process_ecode_IDM_Glacial(void)
{
  MTRACE("IDM_Glacial\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_CLOCK;
  csbMessage.p2 = SPEED_GLACIAL;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0xa);
  };
}

void Process_ecode_IDM_Molasses(void)
{
  MTRACE("IDM_Molasses\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_CLOCK;
  csbMessage.p2 = SPEED_MOLASSES;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0xb);
  };
}

void Process_ecode_IDM_VerySlow(void)
{
  MTRACE("IDM_VerySlow\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_CLOCK;
  csbMessage.p2 = SPEED_VERYSLOW;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0xc);
  };
}

void Process_ecode_IDM_Slow(void)
{
  MTRACE("IDM_Slow\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_CLOCK;
  csbMessage.p2 = SPEED_SLOW;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0xd);
  };
}

void Process_ecode_IDM_Normal(void)
{
  MTRACE("IDM_Normal\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_CLOCK;
  csbMessage.p2 = SPEED_NORMAL;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0xe);
  };
}

void Process_ecode_IDM_Fast(void)
{
  MTRACE("IDM_Fast\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_CLOCK;
  csbMessage.p2 = SPEED_FAST;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0xf);
  };
}

void Process_ecode_IDM_Quick(void)
{
  MTRACE("IDM_Quick\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_CLOCK;
  csbMessage.p2 = SPEED_QUICK;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x10);
  };
}

void Process_ecode_IDM_PlayerClock(void)
{
  MTRACE("IDM_PlayerClock\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_PLAYERCLOCK;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x11);
  };
}

void Process_ecode_IDM_ExtraTicks(void)
{
  MTRACE("IDM_ExtraTicks\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_EXTRATICKS;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x12);
  };
}

void Process_ecode_IDC_Record(void)
{
  MTRACE("IDC_Record\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_RECORD;
  csbMessage.p2 = 1;
  if (RecordMenuOption) csbMessage.p2 = 0;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x13);
  };
}

void Process_ecode_IDC_TimerTrace(void)
{
  MTRACE("IDC_TimerTrace\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_TIMERTRACE;
  csbMessage.p2 = 1;
  if (TimerTraceActive) csbMessage.p2 = 0;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x14);
  };
}

void Process_ecode_IDM_DMRules(void)
{
   MTRACE("IDM_DMRULES\n");
   csbMessage.type = UIM_SETOPTION;
   csbMessage.p1 = OPT_DMRULES;
   if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
   {
     PostQuitMessage(0x15);
   };
}

void Process_ecode_IDC_AttackTrace(void)
{
  MTRACE("IDC_AttackTrace\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_ATTACKTRACE;
  csbMessage.p2 = 1;
  if (AttackTraceActive) csbMessage.p2 = 0;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x16);
  };
}

void Process_ecode_IDC_AITrace(void)
{
  MTRACE("IDC_AITrace\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_AITRACE;
  csbMessage.p2 = 1;
  if (AITraceActive) csbMessage.p2 = 0;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x17);
  };
}

void Process_ecode_IDC_ItemsRemaining(void)
{
  MTRACE("IDC_ItemsRemaining\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_ITEMSREMAINING;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x18);
  };
}

void Process_ecode_IDC_NonCSBItemsRemaining(void)
{
  MTRACE("IDC_NonCSBItemsRemaining\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_NONCSBITEMSREMAINING;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x19);
  };
}

void Process_ecode_IDC_Playback(void)
{
  MTRACE("IDC_Playback\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_PLAYBACK;
  csbMessage.p2 = 1;
  if (PlayfileIsOpen()) csbMessage.p2 = 0;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x1a);
  };
}

void Process_ecode_IDC_DispatchTrace(void)
{
  MTRACE("IDC_DispatchTrace\n");
  if (trace  >= 0 )
  {
    CLOSE(trace);
    trace = -1;
  }
  else
  {
    trace = CREATE("trace.log","w", true);
  };
}

void Process_ecode_IDM_GraphicTrace(void)
{
  MTRACE("IDC_GraphicTrace\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_GRAPHICTRACE;
  csbMessage.p2 = 1;
  if (TimerTraceActive) csbMessage.p2 = 0;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x1b);
  };
}

void Process_ecode_IDC_DSATrace(void)
{
  MTRACE("IDC_DSATrace\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_DSATRACE;
  csbMessage.p2 = 1;
  if (DSATraceActive) csbMessage.p2 = 0;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x1c);
  };
}


void Process_SDL_USEREVENT(void)
{
  MTRACE("sdl_userevent->");
  SDL_UserEvent *e = (SDL_UserEvent*) &evert;
  switch( e->code )
  {// the menu (and timer)
    case IDC_Normal:        Process_ecode_IDC_Normal();        break;
    case IDC_Timer:         Process_ecode_IDC_Timer();         break;
    case IDC_VIDEOEXPOSE:   Process_ecode_IDC_VIDEOEXPOSE();   break;
    case IDC_Double:        Process_ecode_IDC_Double();        break;
    case IDC_Triple:        Process_ecode_IDC_Triple();        break;
    case IDC_Quadruple:     Process_ecode_IDC_Quadruple();     break;
    case IDC_Quintuple:     Process_ecode_IDC_Quintuple();     break;
    case IDC_Sextuple:      Process_ecode_IDC_Sextuple();      break;
    case IDC_QuickPlay:     Process_ecode_IDC_QuickPlay();     break;
    case IDM_Glacial:       Process_ecode_IDM_Glacial();       break;
    case IDM_Molasses:      Process_ecode_IDM_Molasses();      break;
    case IDM_VerySlow:      Process_ecode_IDM_VerySlow();      break;
    case IDM_Slow:          Process_ecode_IDM_Slow();          break;
    case IDM_Normal:        Process_ecode_IDM_Normal();        break;
    case IDM_Fast:          Process_ecode_IDM_Fast();          break;
    case IDM_Quick:         Process_ecode_IDM_Quick();         break;
    case IDM_PlayerClock:   Process_ecode_IDM_PlayerClock();   break;
    case IDM_ExtraTicks:    Process_ecode_IDM_ExtraTicks();    break;
    case IDC_Record:        Process_ecode_IDC_Record();        break;
    case IDC_TimerTrace:    Process_ecode_IDC_TimerTrace();    break;
    case IDM_DMRULES:       Process_ecode_IDM_DMRules();       break;
    case IDC_AttackTrace:   Process_ecode_IDC_AttackTrace();   break;
    case IDC_AITrace:       Process_ecode_IDC_AITrace();       break;
    case IDC_ItemsRemaining:Process_ecode_IDC_ItemsRemaining();break;
    case IDC_Playback:      Process_ecode_IDC_Playback();      break;
    case IDC_DispatchTrace: Process_ecode_IDC_DispatchTrace(); break;
    case IDM_GraphicTrace:  Process_ecode_IDM_GraphicTrace();  break;
    case IDC_DSATrace:      Process_ecode_IDC_DSATrace();      break;
    case IDC_NonCSBItemsRemaining:
                            Process_ecode_IDC_NonCSBItemsRemaining();
                                                               break;
    default:
      printf("Unknown SDL_USEREVENT\n");
      MTRACE("Unknown SDL_USEREVENT\n");
      //return DefWindowProc(hWnd, message, wParam, lParam);
      break;
  };
}

void Process_SDL_MOUSEBUTTONDOWN(void)
{
  MTRACE("SDL_MOUSEBUTTONDOWN->");
  {
    SDL_MouseButtonEvent  *e = (SDL_MouseButtonEvent*) &evert;
    switch(e->button)
    {// btn down, left or right
      case SDL_BUTTON_LEFT:
        MTRACE("SDL_BUTTON_LEFT\n");
        //printf("left button @%d ", (ui32)UI_GetSystemTime());
        csbMessage.type=UIM_LEFT_BUTTON_DOWN;
        csbMessage.p1 = absMouseX;
        csbMessage.p2 = absMouseY;
        if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
        {
          PostQuitMessage(0x1d);
        };
        break;
      case SDL_BUTTON_RIGHT:
        MTRACE("SDL_BUTTON_RIGHT\n");
        //printf("right ");
        csbMessage.type=UIM_RIGHT_BUTTON_DOWN;
        csbMessage.p1 = absMouseX; // = e->x;  // horizontal position of cursor
        csbMessage.p2 = absMouseY; // = e->y;    // vertical position of cursor
        if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
        {
          PostQuitMessage(0x1e);
        };
        break;
    };
  };
  //printf("at (%x, %x)\n", X_TO_CSB(mouseX,screenSize),Y_TO_CSB(mouseY,screenSize));
}

void Process_SDL_MOUSEBUTTONUP(void)
{
  MTRACE("SDL_MOUSEBUTTONUP->");
  {// btn up, left or right
    SDL_MouseButtonEvent  *e = (SDL_MouseButtonEvent*) &evert;
    switch(e->button)
    {
      case SDL_BUTTON_RIGHT:
        MTRACE("SDL_BUTTON_RIGHT\n");
        csbMessage.type=UIM_RIGHT_BUTTON_UP;
        csbMessage.p1 = absMouseX; // = e->x;  // horizontal position of cursor
        csbMessage.p2 = absMouseY; // = e->y; // vertical position of cursor
        if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
        {
          PostQuitMessage(0x1f);
        };
        break;
      case SDL_BUTTON_LEFT:
        MTRACE("SDL_BUTTON_LEFT\n");
        csbMessage.type=UIM_LEFT_BUTTON_UP;
        csbMessage.p1 = absMouseX; // = e->x;   // horizontal position of cursor
        csbMessage.p2 = absMouseY; // = e->y;  // vertical position of cursor
        if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
        {
          PostQuitMessage(0x20);
        };
        break;
    };
  };
}

void Process_SDL_KEYDOWN(void)
{
//  static int prevp1 = -1;
  MTRACE("SDL_KEYDOWN\n");
  {
    /*SDL_KeyboardEvent *e = (SDL_KeyboardEvent*) &evert;
    if(e->keysym->sym==SDLK_LEFT)
    printf("left ");
    if else(e->keysym->sym==SDLK_RIGHT)
    printf("right ");
    */
    csbMessage.type=UIM_KEYDOWN;
    csbMessage.p1 =  evert.key.keysym.sym; //virtual key
    csbMessage.p2 = evert.key.keysym.scancode; //scancode
    //printf("key press %x %x mod %x\n",csbMessage.p1,csbMessage.p2,evert.key.keysym.mod);
    //printf("Key 0x%x pressed @%d\n",(int)csbMessage.p1, (ui32)UI_GetSystemTime());
//    if ((csbMessage.p1 == 3) && (prevp1 == 3))
//    {
//      PostQuitMessage(0);
//      return;
//    };
//    prevp1 = csbMessage.p1;
    if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
    {
      PostQuitMessage(0x21);
    };
  };
  {
    static int prevKeyUp = -1;
    csbMessage.type=UIM_CHAR;
    csbMessage.p1 = evert.key.keysym.sym;
    if (evert.key.keysym.sym == SDLK_LCTRL) return;
    if (evert.key.keysym.sym == SDLK_RCTRL) return;
    //{
    //  char line[80];
    //  sprintf(line, "p1=%d; mod=%d", csbMessage.p1, evert.key.keysym.mod);
    //  UI_MessageBox(line, "KEYUP", MESSAGE_OK);
    //};
    if (evert.key.keysym.mod &  (KMOD_LCTRL | KMOD_RCTRL))
    {
      if (csbMessage.p1 == SDLK_c)
      {
        if (prevKeyUp == 3)
        {
          PostQuitMessage(0x22);
          return;
        };
        csbMessage.p1 = 3; //Control-c
      };
    };
#if defined SDL20
    if ((evert.key.keysym.mod & (KMOD_ALT)) && evert.key.keysym.sym == SDLK_RETURN) {
	static int oldw, oldh;
	SDL_DisplayMode mode;
	SDL_GetDesktopDisplayMode(0,&mode);

	if (fullscreenActive) {
	    fullscreenActive = 0;
	} else {
	    fullscreenActive = SDL_WINDOW_FULLSCREEN_DESKTOP;
	    SDL_SetWindowDisplayMode(sdlWindow,&mode);
	}
	SDL_SetWindowFullscreen(sdlWindow,fullscreenActive);
    }
#endif

    prevKeyUp = csbMessage.p1;
    if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
    {
      PostQuitMessage(0x23);
    };
  };
}

void Process_SDL_KEYUP(void)
{
  MTRACE("SDL_KEYUP\n");
}

#ifdef SDL20

void Process_SDL_WINDOWEVENT(void)
{
  switch(evert.window.event)
  {
    case SDL_WINDOWEVENT_SHOWN:
      break;
    case SDL_WINDOWEVENT_HIDDEN:
      break;
    case SDL_WINDOWEVENT_LEAVE:
      break;
    case SDL_WINDOWEVENT_MOVED:
      break;
    case SDL_WINDOWEVENT_SIZE_CHANGED:
      break;
    case SDL_WINDOWEVENT_RESIZED:
      __resize_screen( evert.window.data1, evert.window.data2 );
      break;
    case SDL_WINDOWEVENT_EXPOSED:
      csbMessage.type=UIM_REDRAW_ENTIRE_SCREEN;
      if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
      {
	  PostQuitMessage(0x24);
      }
      break;
    case SDL_WINDOWEVENT_ENTER:
      break;
    case SDL_WINDOWEVENT_FOCUS_GAINED:
      break;
    case SDL_WINDOWEVENT_FOCUS_LOST:
      break;
    case SDL_WINDOWEVENT_MINIMIZED:
      break;
    case SDL_WINDOWEVENT_MAXIMIZED:
      break;
    case SDL_WINDOWEVENT_RESTORED:
      break;
    case SDL_WINDOWEVENT_TAKE_FOCUS:
      break;
    case SDL_WINDOWEVENT_CLOSE:
      PostQuitMessage(1);
      break;
    default:
      {
        char line[80];
        sprintf(line,
                "evert.window.event = %d\n",
                evert.window.event);
        UI_MessageBox(line,
                      "Process_SDL_WINDOWEVENT",
                      MESSAGE_OK);
        NotImplemented(0x66); break;
        // check for RESIZE
    };
  };
}

#endif
ImGuiIO io;
static int drawn = 0;
void post_render();

/********************** MAIN **********************/
int main (int argc, char* argv[])
{
#ifdef MAEMO_NOKIA_770
  HildonProgram* program;
#endif//MAEMO_NOKIA_770
  /* Set default 'root' directory. */
  //folderSavedGame=(char*)".";
//  {
//    FILE *f;
//    f = fopen("/run/shm/debug", "w");
//    fprintf(f,"Debugging info\n");
//    fclose(f);
//  };
  folderParentName=(char*)".";
  folderName=(char*)".";
  root = (char*)".";
  /* Parse commandline arguments.
   * Eat all arguments I recognize. Otherwise Gnome kills me.
   */
  printf("%s\n", APPTITLE  " " APPVERSION APPVERMINOR );
        versionSignature = Signature(szCSBVersion);
  if( UI_ProcessOption(argv,argc) == false )
  {
      exit(0);
  }
  if( !(folderParentName&&folderName&&root) )
  {
    root=(char*)"./";
  }

  //    ***** Init the Aplication enviroment ******

#ifdef USE_OLD_GTK
  gtk_init (&argc, &argv);
  appGlobal = gtk_window_new (GTK_WINDOW_TOPLEVEL);

#ifdef MAEMO_NOKIA_770
  /* Create the hildon application and setup the title */
  program = HILDON_PROGRAM ( hildon_program_get_instance () );
  hildonmainwindow = HILDON_WINDOW(hildon_window_new());
  g_set_application_name ( "CSB" );
  //    g_signal_connect(G_OBJECT(hildonmainwindow), "delete_event", gtk_main_quit, NULL);
  hildon_program_add_window(program, hildonmainwindow);

  appGlobal = GTK_WIDGET(hildonmainwindow); //gtk_window_new (GTK_WINDOW_TOPLEVEL);
#endif//MAEMO_NOKIA_770

  if (appGlobal == NULL)
  {
    g_error("main: GTK application is a nullpointer.");
  }
  gtk_window_set_title (GTK_WINDOW (appGlobal), APPTITLE);
  //gtk_signal_connect (GTK_OBJECT (appGlobal), "delete_event",GTK_SIGNAL_FUNC (cbAppDestroy), NULL);
  gtk_signal_connect (GTK_OBJECT (appGlobal), "destroy",GTK_SIGNAL_FUNC (cbAppDestroy), NULL);
  gtk_signal_connect (GTK_OBJECT (appGlobal), "expose_event",GTK_SIGNAL_FUNC (__timer_callback),(void*)( IDC_VIDEOEXPOSE));

  //gtk_create_menu
  {
    GtkItemFactory *item_factory;
    GtkAccelGroup *accel_group;
    GtkWidget *menuW;
    GtkWidget *main_vbox;
    static const gint nmenu_items = sizeof (menubar) / sizeof (menubar[0]);

    accel_group = gtk_accel_group_new ();
    item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>",
                                       accel_group);
    gtk_item_factory_create_items (item_factory, nmenu_items, menubar, NULL);
    gtk_window_add_accel_group (GTK_WINDOW (appGlobal), accel_group);
    gtk_signal_connect(GTK_OBJECT(gtk_item_factory_get_widget(item_factory,"/Misc")),
                       "expose_event",
    GTK_SIGNAL_FUNC(__before_misc_menu_is_showed),
                     (gpointer) item_factory);
    gtk_signal_connect(GTK_OBJECT(gtk_item_factory_get_widget(item_factory,"/Trace")),
                      "expose_event",
    GTK_SIGNAL_FUNC(__before_trace_menu_is_showed),
                    (gpointer) item_factory);
    menuW = gtk_item_factory_get_widget (item_factory, "<main>");
    main_vbox = gtk_vbox_new (FALSE, 1);
    gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 1);
    gtk_container_add (GTK_CONTAINER (appGlobal), main_vbox);
    gtk_box_pack_start (GTK_BOX (main_vbox), menuW, FALSE, TRUE, 0);

    gtk_check_menu_item_set_active  (GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(item_factory,"/Misc/Size x 2")), true);// Default is '2 x ScreenSize'
    gtk_check_menu_item_set_active  (GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(item_factory,"/Speed/Normal")), true); // Default is 'Normal' speed
    if(!PlaybackCommandOption)
    {
      gtk_check_menu_item_set_active  (GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(item_factory,"/Speed/Extra Ticks")), true);extraTicks=true;// Default is 'Extra Ticks'
    };
  };


  gtk_widget_show_all (appGlobal);
#ifdef MAEMO_NOKIA_770
  /* Hack to get SDL to use GTK window */
  /* Doesn't work with gtk-1.2 */
  {
    char SDL_windowhack[32];
    sprintf( SDL_windowhack,"SDL_WINDOWID=%ld",
            (GTK_WINDOW(hildonmainwindow)));
            (GTK_WINDOW(appGlobal)));

    //      GDK_WINDOW_XWINDOW(appGlobal->window));
    putenv(SDL_windowhack);
    printf("Done SDL window hack. ID should be %ld\n%s", GDK_WINDOW_XWINDOW(appGlobal->window), SDL_windowhack);
  };
#endif //MAEMO_NOKIA_770
#endif //USE_OLD_GTK

//    ***** Initialize defaults, Video and Audio *****
  if ( SDL_Init ( SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER|SDL_INIT_EVENTS)<0)//|SDL_INIT_AUDIO|SDL_INIT_TIMER) < 0)
  // if ( SDL_Init ( SDL_INIT_VIDEO)<0)//|SDL_INIT_AUDIO|SDL_INIT_TIMER) < 0)
  {
    fprintf(stderr,"Unable to init SDL: %s", SDL_GetError() );
    //g_error("Unable to init SDL: %s", SDL_GetError() );
  };
  printf("SDL initialized.\n");
#if defined SDL20
  SDL_SetWindowTitle(sdlWindow,szCSBVersion);
#else
  xxxError
#endif


// ****************************** the DISPLAY ****************************************************
  {
#if defined SDL20
    int flags;
    screenSize = 1;
    flags = 0;
    if (fullscreenRequested)
    {
      flags =  SDL_WINDOW_FULLSCREEN_DESKTOP;
    };
    flags |= SDL_WINDOW_RESIZABLE;
    SDL_GetDisplayBounds(0,&desktop);
    if ((sdlWindow = SDL_CreateWindow(
                   szCSBVersion,
                   SDL_WINDOWPOS_CENTERED,
                   SDL_WINDOWPOS_CENTERED,
                   WindowWidth,WindowHeight,
                   flags)) == NULL)
    {
      UI_MessageBox(SDL_GetError(),
                    "Failed to create window",
                    MESSAGE_OK);
      die(0xe9a3);
    };
    SDL_Surface *sf = SDL_LoadBMP("lsb.bmp");
    if (sf) {
	SDL_SetWindowIcon(sdlWindow,sf);
	SDL_FreeSurface(sf);
    }
    SDL_VIDEOEXPOSE = SDL_RegisterEvents(1);
    if ((sdlRenderer = SDL_CreateRenderer(
                   sdlWindow,
                   -1,
                   0)) == NULL)
    {
      UI_MessageBox(SDL_GetError(),
                    "Failed to create Renderer",
                    MESSAGE_OK);
      die(0x519b);
    };
    // I disable this because of imgui, the game picture doesn't take the whole window anymore, so it's better to convert the coordinates ourselves
    // SDL_RenderSetLogicalSize(sdlRenderer,320,200);
    if ((sdlTexture = SDL_CreateTexture(
                   sdlRenderer,
                   SDL_PIXELFORMAT_RGB565,
                   SDL_TEXTUREACCESS_STREAMING,
                   320,200)) == NULL)
    {
      UI_MessageBox(SDL_GetError(),
                    "Failed to create texture",
                    MESSAGE_OK);
      die(0xff53);
    };
#else
    xxxError
#endif
#ifdef _DYN_WINSIZE
    if(!Hermes_Init()) g_error("No hermes...");
    from_palette = Hermes_PaletteInstance();
    to_palette = Hermes_PaletteInstance();
    converter = Hermes_ConverterInstance( HERMES_CONVERT_NORMAL );
    from_format = Hermes_FormatNew(8,0,0,0,0,TRUE);
#else
#ifdef SDL12
    SCRAP = SDL_CreateRGBSurface(SDL_SWSURFACE,screenWidth,screenHeight,8,0,0,0,0);
    {
      FILE *f;
      f = fopen("/run/shm/debug", "a");
      fprintf(f, "Creating Surface %d %d\n", screenWidth, screenHeight);
      fclose(f);
    };
    if (SCRAP == NULL)
    {
      g_error("Unable to get SDL surface: %s",SDL_GetError());
    };
#endif
#endif
  };

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsLight();

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForSDLRenderer(sdlWindow, sdlRenderer);
  ImGui_ImplSDLRenderer2_Init(sdlRenderer);

  // Load Fonts
  io.Fonts->AddFontFromFileTTF("fonts/Vera.ttf", FONT_SIZE);

  SDL_ShowCursor(SDL_ENABLE);
  cursorIsShowing = true;
  if(fullscreenRequested)
  {
    fullscreenActive = true;
#if defined SDL12
    //SDL_WM_ToggleFullScreen(WND);
#elif defined SDL20
    //NotImplemented(0x84ea);
#else
    xxxError
#endif
  }
  UI_Initialize_sounds();


  /*
   * Initialize the display in a 640x480 8-bit palettized mode,
   * requesting a software surface, or anything else....
   */

  /* Do the window-cha-cha. */
  speedTable[SPEED_GLACIAL].vblPerTick = 1000;
  speedTable[SPEED_MOLASSES].vblPerTick = 55;
  speedTable[SPEED_VERYSLOW].vblPerTick = 33;
  speedTable[SPEED_SLOW].vblPerTick = 22;
  speedTable[SPEED_NORMAL].vblPerTick = 15;
  speedTable[SPEED_FAST].vblPerTick = 11;
  speedTable[SPEED_QUICK].vblPerTick = 7;
  speedTable[SPEED_FTL].vblPerTick = 5;

  volumeTable[VOLUME_FULL].divisor = 1;
  volumeTable[VOLUME_HALF].divisor = 2;
  volumeTable[VOLUME_QUARTER].divisor = 4;
  volumeTable[VOLUME_EIGHTH].divisor = 8;
  volumeTable[VOLUME_OFF].divisor = 65536;

  MTRACE("*** First initialization ***\n");
  csbMessage.type=UIM_INITIALIZE;
  /* Parse config.linux */
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x25);
  };

  /* Setup a 50ms timer. */
  sem = SDL_CreateSemaphore(1);
  timer = SDL_AddTimer(TImER?TImER:10, __Timer_Callback, (void*)(IDC_Timer));

  //SDL_WM_GrabInput(SDL_GRAB_ON);

  /********************************************
   *
   *       Main Execution Loop
   *
   ********************************************
   */
  while (true)
  {

#ifdef USE_OLD_GTK
    gtk_main_iteration_do(FALSE); // This solution will always cost CPU instead of gtk_main()
#endif //USE_OLD_GTK
    // evert = app.WaitEvent();
    if (SDL_PollEvent(&evert) == 0)
    {
	SDL_SemWait(sem);
	timerInQueue=false;
	SDL_SemPost(sem);
      if(SDL_WaitEvent(&evert) == 0) {
	  printf("SDL_WaitEvent error %s\n",SDL_GetError());
	  continue;
      }
    }

    ImGui_ImplSDL2_ProcessEvent(&evert);
    if (imgui_active) {
	switch(evert.type) {
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
	case SDL_KEYDOWN:
	case SDL_KEYUP:
	case SDL_MOUSEMOTION: continue;
	}
    }
    // uint32_t tick = SDL_GetTicks();
    // printf("event type %x ticks %d\n",evert.type,tick);
    if (sdlQuitPending)
    {
//      FILE *f;
//      f = fopen("debug","a");
//      fprintf(f, "MainLook sdlQuitPending type= %d\n", evert.type);
//      fclose(f);
      if (SDL_QuitRequested())
      {
        if (evert.type != SDL_QUIT) continue;
      }
      else
      {
        sdlQuitPending = false;
      };
    };
    /* Listen for 'quick buttons' here. */
    /* Hail to the Great Message Struct! */
    MTRACE("msg=");
    switch( evert.type )
    {
      case SDL_QUIT:
        MTRACE("SDL_QUIT\n");
        cbAppDestroy();
        break;
      default:
        MTRACE("was ignored\n");
        break;
      case SDL_MOUSEMOTION:     Process_SDL_MOUSEMOTION(
                                         cursorIsShowing);
                                break;
      case SDL_USEREVENT:       Process_SDL_USEREVENT();       break;// __timer_loopback
      case SDL_MOUSEBUTTONDOWN: Process_SDL_MOUSEBUTTONDOWN(); break;
      case SDL_MOUSEBUTTONUP:   Process_SDL_MOUSEBUTTONUP();   break;
      case SDL_KEYDOWN:         Process_SDL_KEYDOWN();         break;
      case SDL_KEYUP:           Process_SDL_KEYUP();           break;
      case SDL_WINDOWEVENT:     Process_SDL_WINDOWEVENT();    break;
      /*
      case SDL_ACTIVEEVENT:
        {
          if ( evert.active.state & SDL_APPACTIVE )
          {
            if ( evert.active.gain )
            {
              printfalll("App activated\n");
            }
            else
            {
              printf("App iconified\n");
            };
          };
        };
      */

    }; /* Eof Great Event Switch */
    if (imgui_active) {
	if (drawn) {
	    drawn = 0;
	} else {
	    post_render();
	    drawn = 0;
	}
    }

  } /* Eof Lord Message Loop */

  Cleanup(true);
  done_imgui();
  doneCaches();
  printf("Quiting SDL.\n");

  /* Shutdown all subsystems */
  SDL_Quit();
  printf("Quiting....\n");
  return (0);
}

extern void ItemsRemaining(i32 mode); // CSBUI.cpp
extern const char *listing_title; // LinCSBUI.cpp
extern LISTING *listing; // AsciiDump.cpp
extern bool resetstartcsb;
bool show_listing;
extern i32 lastTime;
extern bool skipLogo;

static void reset_game() {
    verticalIntEnabled = false;
    lastTime = 0; // reset _MouseHandleEvents
    SDL_ShowCursor(SDL_ENABLE);
    cursorIsShowing = true;
    Cleanup(false);
    resetWhatToDo=resetgamesetup=resetstartcsb=resetprisondoor=true;
    skipLogo = true;
    csbMessage.type=UIM_INITIALIZE;
    if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
    {
	PostQuitMessage(0);
    }
}

static bool fb_shown,fb2_shown,fb3_shown;
extern bool chaosDisplayed,skipToDungeon,skipToResumeGame; // CSBCode.cpp

void post_render() {
    static bool was_active;
    drawn = 1;
    SDL_Rect r;
    static bool show_coords;
    bool open = false,save = false,open_dungeon = false;
    if (!d.NumGraphic) imgui_active = true;

    // Start the Dear ImGui frame
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (chaosDisplayed) ImGui::BeginDisabled();
    if (ImGui::BeginMainMenuBar())
    {
	// I don't know why you should use IsWindowHovered and not IsItemHovered for a mainmenubar, but that's the way it is...
	bool on_menubar = ImGui::IsWindowHovered();
	if (on_menubar) {
	    imgui_active = true;
	    was_active = true;
	}
	r.y = DY;
	r.x = 0;
	r.w = WindowWidth;
	r.h = WindowHeight - DY;
	if (ImGui::BeginMenu(_("File")))
	{
	    imgui_active = true;
	    was_active = true;
	    if (ImGui::MenuItem("Reset", NULL,false,d.partyLevel != 255 && d.NumGraphic > 0)) {
		reset_game();
		// _CALL0 (_4_,st_ReadEntireGame);
	    }
	    if (ImGui::MenuItem(_("Select dungeon.dat..."),NULL,false,d.partyLevel != 255))
		open_dungeon = true;
	    if (ImGui::MenuItem(_("Load saved game"),NULL,false,d.partyLevel != 255 && d.NumGraphic>0))
		open = true;
	    if (ImGui::MenuItem(_("Save game"),NULL,false,d.partyLevel != 255 && d.NumGraphic>0))
		save = true;
	    // Limited playback in front the dungeon door
	    if (ImGui::MenuItem(_("Playback.."), NULL,false,d.partyLevel == 255 && d.NumGraphic > 0)) { Process_ecode_IDC_Playback();/* Do stuff */ }
	    if (ImGui::MenuItem(_("Quit"), NULL))   { cbAppDestroy(); }
	    ImGui::EndMenu();
	} else if (!fb_shown && !on_menubar && !fb2_shown && !fb3_shown && d.NumGraphic)
	    imgui_active = false;

	if (ImGui::BeginMenu(_("Speed"))) {
	    imgui_active = true;
	    was_active = true;
	    if (ImGui::MenuItem(_("Glacial"),NULL,gameSpeed == SPEED_GLACIAL))
		gameSpeed = SPEED_GLACIAL;
	    if (ImGui::MenuItem(_("Molasses"),NULL,gameSpeed == SPEED_MOLASSES))
		gameSpeed = SPEED_MOLASSES;
	    if (ImGui::MenuItem(_("Very Slow"),NULL,gameSpeed == SPEED_VERYSLOW))
		gameSpeed = SPEED_VERYSLOW;
	    if (ImGui::MenuItem(_("Slow"),NULL,gameSpeed == SPEED_SLOW))
		gameSpeed = SPEED_SLOW;
	    if (ImGui::MenuItem(_("Normal"),NULL,gameSpeed == SPEED_NORMAL))
		gameSpeed = SPEED_NORMAL;
	    if (ImGui::MenuItem(_("Fast"),NULL,gameSpeed == SPEED_FAST))
		gameSpeed = SPEED_FAST;
	    if (ImGui::MenuItem(_("Quick as a bunny"),NULL,gameSpeed == SPEED_QUICK))
		gameSpeed = SPEED_QUICK;
	    if (ImGui::MenuItem(_("Faster Than Light"),NULL,gameSpeed == SPEED_FTL))
		gameSpeed = SPEED_FTL;
	    ImGui::Separator();
	    ImGui::MenuItem(_("Extra Ticks"),NULL,&extraTicks);
	    ImGui::MenuItem(_("Player Clock"),NULL,&playerClock);
	    ImGui::EndMenu();
	}
	if (ImGui::BeginMenu(_("Misc")))
	{
	    imgui_active = true;
	    was_active = true;
	    ImGui::MenuItem(_("Party coordinates"), NULL,&show_coords);
	    bool enabled = (ItemsRemainingOK
		    && (encipheredDataFile==NULL)
		    && !simpleEncipher);
	    if (ImGui::MenuItem(_("Non-CSB Items"), NULL,false,enabled)) ItemsRemaining(1);
	    ImGui::MenuItem(_("DM Rules"), NULL,&DM_rules);
	    ImGui::EndMenu();
	} else if (!imgui_active) {
	    if (was_active && !cursorIsShowing) {
		SDL_ShowCursor(SDL_DISABLE);
		// Tried RemoveCursor / ShowCursor, problem is last call to ShowCursor leaves a permanent cursor on game screen
		// so it's better to fight a little with mouse cursors when opening the mneu for now
		// until I find something better
		was_active = false;
	    }
	}

	if (show_coords) {
	    ImGui::SameLine(ImGui::GetWindowWidth() - 100);
	    ImGui::Text("Party %d,%d,%d",d.partyLevel,d.partyX,d.partyY);
	}
	ImGui::EndMainMenuBar();
    }
    if (chaosDisplayed) ImGui::EndDisabled();

    //Remember the name to ImGui::OpenPopup() and showFileDialog() must be same...
    if(open) {
	fb_shown = true;
	ImGui::OpenPopup(_("Load saved game"));
    } else if (open_dungeon) {
	fb3_shown = true;
	ImGui::OpenPopup(_("Select dungeon.dat..."));
    } else if (save) {
	fb2_shown = true;
        ImGui::OpenPopup(_("Save game"));
    }

    /* Optional third parameter. Support opening only compressed rar/zip files.
     * Opening any other file will show error, return false and won't close the dialog.
     */
    if(file_dialog.showFileDialog(_("Load saved game"), imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(700, 310), "csb*"))
    {
	printf("file %s\n",file_dialog.selected_fn.c_str());    // The name of the selected file or directory in case of Select Directory dialog mode
	printf("path %s\n",file_dialog.selected_path.c_str());  // The absolute path to the selected file
	opened_file = (char*)file_dialog.selected_path.c_str();
	skipToDungeon = true;
	skipToResumeGame = true;
	reset_game();
	fb_shown = false;
    } else if (fb_shown && !ImGui::IsPopupOpen(_("Load saved game"))) {
	// cancel was pressed!
	fb_shown = false;
    }

    if(file_dialog.showFileDialog(_("Select dungeon.dat..."), imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(700, 310), "dungeon.dat,*.dat"))
    {
	printf("file %s\n",file_dialog.selected_fn.c_str());    // The name of the selected file or directory in case of Select Directory dialog mode
	printf("path %s\n",file_dialog.selected_path.c_str());  // The absolute path to the selected file
	dungeonName = (char*)file_dialog.selected_path.c_str();
	root = (char*)"";
	skipToDungeon = true;
	reset_game();
	fb3_shown = false;
    } else if (fb3_shown && !ImGui::IsPopupOpen(_("Select dungeon.dat...")))
	fb3_shown = false;

    if(file_dialog.showFileDialog(_("Save game"), imgui_addons::ImGuiFileBrowser::DialogMode::SAVE, ImVec2(700, 310), "csb*"))
    {
	opened_file = (char*)file_dialog.selected_path.c_str();
	fb2_shown = false;
	DispatchCSB(st_DisplayDiskMenu);
    } else if (fb2_shown && !ImGui::IsPopupOpen(_("Save game"))) {
	// cancel was pressed!
	fb2_shown = false;
    }
    if (listing && listing_title && show_listing) {
	if (ImGui::Begin(listing_title, &show_listing)) {
	    if (ImGui::Button(_("Save to report.txt"))) {
		FILE *f = fopen("report.txt","w");
		if (f) {
		    fprintf(f,"%s\n",listing->m_listing);
		    fclose(f);
		}
	    }

	    if (ImGui::IsWindowHovered()) {
		SDL_ShowCursor(SDL_ENABLE);
		imgui_active = true;
	    }
	    ImGui::Text("%s",listing->m_listing);
	    ImGui::End();
	} else {
	    printf("window collapsed or closed\n");
	    ImGui::End();
	}
    }
    if (!was_active && !cursorIsShowing && !fb_shown && !imgui_active && !fb2_shown && !fb3_shown) {
	SDL_ShowCursor(SDL_DISABLE);
    }

    // Rendering
    ImGui::Render();

    SDL_RenderSetScale(sdlRenderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(sdlRenderer);

    double ratio = r.w*1.0/r.h;
    if (abs(ratio - 32/20.0) > 1e-5) {
	if (ratio > 32/20.0) {
	    r.w = r.h*32/20.0;
	    r.x = (WindowWidth-r.w)/2;
	} else {
	    r.h = r.w*20/32.0;
	    r.y = (WindowHeight - r.h)/2;
	}
    }
    if (d.NumGraphic>0 && SDL_RenderCopy(sdlRenderer,
		sdlTexture,
		NULL,
		&r) < 0) {
	printf("rendercopy error %s\n",SDL_GetError());
    }
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),sdlRenderer);
    SDL_RenderPresent(sdlRenderer);
}
