// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	DOOM selection menu, options, episode etc.
//	Sliders and icons. Kinda widget stuff.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: m_menu.c,v 1.7 1997/02/03 22:45:10 b1 Exp $";

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>


#include "doomdef.h"
#include "dstrings.h"

#include "d_main.h"

#include "i_system.h"
#include "i_sound.h"
#include "i_net.h"
#include "i_video.h"
#include "z_zone.h"
#include "v_video.h"
#include "w_wad.h"

#include "r_local.h"


#include "hu_stuff.h"

#include "g_game.h"

#include "m_argv.h"
#include "m_swap.h"

#include "s_sound.h"

#include "doomstat.h"

// Data.
#include "sounds.h"

#include "m_menu.h"



extern patch_t*		hu_font[HU_FONTSIZE];
extern boolean		message_dontfuckwithme;

extern boolean		chat_on;		// in heads-up code

//
// defaulted values
//
int			mouseSensitivity;       // has default

// Show messages has default, 0 = off, 1 = on
int			showMessages;
	

// Blocky mode, has default, 0 = high, 1 = normal
int			detailLevel;		
int			screenblocks;		// has default

// Multiplayer UI verbosity (0 = friendly, 1 = show detailed session info)
int                     mp_verbose_info;        // has default

// Autosave settings: 0 = disabled, 1 = enabled (saved to slot 5)
int			autosave_enable;        // has default (0 = off)

// Multiplayer lobby runtime (Phase 4).
static int mp_lobby_inflight = 0;
static int mp_lobby_launched = 0;
static int mp_lobby_last_status = NET_STATUS_INIT;
static int mp_manual_ip_enter = 0;
static char mp_manual_old_ip[64];

// -1 = no quicksave slot picked!
int			quickSaveSlot;          

 // 1 = message to be printed
int			messageToPrint;
// ...and here is the message string!
char*			messageString;		

// message x & y
int			messx;			
int			messy;
int			messageLastMenuActive;

// timed message = no input from user
boolean			messageNeedsInput;     

void    (*messageRoutine)(int response);

#define SAVESTRINGSIZE 	24

char gammamsg[5][26] =
{
    GAMMALVL0,
    GAMMALVL1,
    GAMMALVL2,
    GAMMALVL3,
    GAMMALVL4
};

// we are going to be entering a savegame string
int			saveStringEnter;              
int             	saveSlot;	// which slot to save in
int			saveCharIndex;	// which char we're editing
// old save description before edit
char			saveOldString[SAVESTRINGSIZE];  

boolean			inhelpscreens;
boolean			menuactive;

#define SKULLXOFF		-32
#define LINEHEIGHT		16

extern boolean		sendpause;
char			savegamestrings[10][SAVESTRINGSIZE];

char	endstring[160];


//
// MENU TYPEDEFS
//
typedef struct
{
    // 0 = no cursor here, 1 = ok, 2 = arrows ok
    short	status;
    
    char	name[10];
    
    // choice = menu item #.
    // if status = 2,
    //   choice=0:leftarrow,1:rightarrow
    void	(*routine)(int choice);
    
    // hotkey in menu
    char	alphaKey;			
} menuitem_t;



typedef struct menu_s
{
    short		numitems;	// # of menu items
    struct menu_s*	prevMenu;	// previous menu
    menuitem_t*		menuitems;	// menu items
    void		(*routine)();	// draw routine
    short		x;
    short		y;		// x,y of menu
    short		lastOn;		// last item user was on in menu
} menu_t;

typedef struct
{
    int player_count;           // 2-4
    int vanilla_only;           // 0 or 1
    int is_host;                // 1 if hosting, 0 if joining
    int connected_players;
    char player_names[MAXPLAYERS][16];
    int start_skill;            // 1-5
    int start_episode;          // 1-4
    int start_map;              // 1-32
} lobby_state_t;

typedef struct
{
    IPaddress servers[8];
    int server_count;
    char manual_ip[64];
    int manual_ip_cursor;
} browser_state_t;

static lobby_state_t lobby_state = {0};
static browser_state_t browser_state = {0};

short		itemOn;			// menu item skull is on
short		skullAnimCounter;	// skull animation counter
short		whichSkull;		// which skull to draw

// graphic name of skulls
// warning: initializer-string for array of chars is too long
char    skullName[2][/*8*/9] = {"M_SKULL1","M_SKULL2"};

// current menudef
menu_t*	currentMenu;                          

//
// PROTOTYPES
//
void M_NewGame(int choice);
void M_Episode(int choice);
void M_ChooseSkill(int choice);
void M_LoadGame(int choice);
void M_SaveGame(int choice);
void M_Options(int choice);
void M_EndGame(int choice);
void M_ReadThis(int choice);
void M_ReadThis2(int choice);
void M_QuitDOOM(int choice);

void M_ChangeMessages(int choice);
void M_ChangeSensitivity(int choice);
void M_SfxVol(int choice);
void M_MusicVol(int choice);
void M_MusicBackend(int choice);
void M_ChangeDetail(int choice);
void M_ChangeAutosave(int choice);
void M_StartGame(int choice);
void M_Sound(int choice);
void M_OpenDisplay(int choice);
void M_OpenNetwork(int choice);
void M_OpenMultiplayer(int choice);
void M_DisplayAspect(int choice);
void M_DisplayScale(int choice);
void M_DisplayResolution(int choice);
void M_DisplayFullscreen(int choice);
void M_NetLatency(int choice);
void M_NetPacketLoss(int choice);
void M_NetVerbose(int choice);

void M_HostSetup(int choice);
void M_JoinSetup(int choice);
void M_HostPlayers(int choice);
void M_HostVanilla(int choice);
void M_HostStart(int choice);
void M_JoinRefresh(int choice);

void M_ViewStatus(int choice);
void M_Resume(int choice);
void M_DrawPauseMenu(void);
void M_JoinSelect(int choice);
void M_JoinManual(int choice);

void M_FinishReadThis(int choice);
void M_LoadSelect(int choice);
void M_SaveSelect(int choice);
void M_ReadSaveStrings(void);
void M_QuickSave(void);
void M_QuickLoad(void);

void M_DrawMainMenu(void);
void M_DrawSinglePlayer(void);
void M_DrawReadThis1(void);
void M_DrawReadThis2(void);
void M_DrawNewGame(void);
void M_DrawEpisode(void);
void M_DrawOptions(void);
void M_DrawSound(void);
void M_DrawDisplay(void);
void M_DrawNetwork(void);
void M_DrawMultiplayer(void);
void M_DrawHostSetup(void);
void M_DrawJoinBrowser(void);
void M_DrawWaitingLobby(void);
void M_DrawLoad(void);
void M_DrawSave(void);

void M_OpenSinglePlayer(int choice);
void M_OpenMainMenu(int choice);
void M_DrawSaveLoadBorder(int x,int y);
void M_SetupNextMenu(menu_t *menudef);
void M_DrawThermo(int x,int y,int thermWidth,int thermDot);
void M_DrawEmptyCell(menu_t *menu,int item);
void M_DrawSelCell(menu_t *menu,int item);
void M_WriteText(int x, int y, char *string);
int  M_StringWidth(char *string);
int  M_StringHeight(char *string);
void M_StartControlPanel(void);
void M_StartMessage(char *string,void *routine,boolean input);
void M_StopMessage(void);
void M_ClearMenus (void);

extern int music_backend;
static const char* GetAspectLabel(void);




//
// DOOM MENU
//
enum
{
    main_singleplayer = 0,
    main_multiplayer,
    main_options,
    main_quit,
    main_end
} main_e;

menuitem_t MainMenu[]=
{
    {1,"",M_OpenSinglePlayer,'s'},
    {1,"",M_OpenMultiplayer,'m'},
    {1,"",M_Options,'o'},
    {1,"",M_QuitDOOM,'q'}
};

menu_t  MainDef =
{
    main_end,
    NULL,
    MainMenu,
    M_DrawMainMenu,
    60,64,
    0
};

//
// PAUSE MENU (shown when ESC pressed during gameplay)
//
enum
{
    pause_status = 0,
    pause_save,
    pause_load,
    pause_options,
    pause_quit,
    pause_back,
    pause_end
} pause_e;

menuitem_t PauseMenu[]=
{
    {1,"", M_ViewStatus,'s'},
    {1,"", M_SaveGame,'s'},
    {1,"", M_LoadGame,'l'},
    {1,"", M_Options,'o'},
    {1,"", M_QuitDOOM,'q'},
    {1,"", M_Resume,'r'}
};

menu_t  PauseDef =
{
    pause_end,
    &MainDef,
    PauseMenu,
    M_DrawPauseMenu,
    60,50,
    0
};

//
// SINGLE PLAYER MENU
//
enum
{
    sp_newgame = 0,
    sp_loadgame,
    sp_savegame,
    sp_back,
    sp_end
} singleplayer_e;

menuitem_t SinglePlayerMenu[]=
{
    {1,"", M_NewGame,'n'},
    {1,"", M_LoadGame,'l'},
    {1,"", M_SaveGame,'s'},
    {1,"", M_OpenMainMenu,'b'}
};

menu_t SinglePlayerDef =
{
    sp_end,
    &MainDef,
    SinglePlayerMenu,
    M_DrawSinglePlayer,
    60,64,
    0
};


//
// EPISODE SELECT
//
enum
{
    ep1,
    ep2,
    ep3,
    ep4,
    ep_end
} episodes_e;

menuitem_t EpisodeMenu[]=
{
    {1,"M_EPI1", M_Episode,'k'},
    {1,"M_EPI2", M_Episode,'t'},
    {1,"M_EPI3", M_Episode,'i'},
    {1,"M_EPI4", M_Episode,'t'}
};

menu_t  EpiDef =
{
    ep_end,		// # of menu items
    &SinglePlayerDef,		// previous menu
    EpisodeMenu,	// menuitem_t ->
    M_DrawEpisode,	// drawing routine ->
    48,63,              // x,y
    ep1			// lastOn
};

//
// NEW GAME
//
enum
{
    killthings,
    toorough,
    hurtme,
    violence,
    nightmare,
    newg_end
} newgame_e;

menuitem_t NewGameMenu[]=
{
    {1,"M_JKILL",	M_ChooseSkill, 'i'},
    {1,"M_ROUGH",	M_ChooseSkill, 'h'},
    {1,"M_HURT",	M_ChooseSkill, 'h'},
    {1,"M_ULTRA",	M_ChooseSkill, 'u'},
    {1,"M_NMARE",	M_ChooseSkill, 'n'}
};

menu_t  NewDef =
{
    newg_end,		// # of menu items
    &EpiDef,		// previous menu
    NewGameMenu,	// menuitem_t ->
    M_DrawNewGame,	// drawing routine ->
    48,63,              // x,y
    hurtme		// lastOn
};



//
// OPTIONS MENU
//
enum
{
    endgame,
    messages,
    detail,
    mousesens,
    option_empty1,
    soundvol,
    option_empty2,
    displayopt,
    networkopt,
    gameinst,
    autosave_opt,
    opt_end
} options_e;

menuitem_t OptionsMenu[]=
{
    {1,"",		M_EndGame,'e'},
    {1,"",		M_ChangeMessages,'m'},
    {1,"",		M_ChangeDetail,'g'},
    {2,"",		M_ChangeSensitivity,'m'},
    {-1,"",0},
    {1,"",		M_Sound,'s'},
    {-1,"",0},
    {1,"",		M_OpenDisplay,'d'},
    {1,"",		M_OpenNetwork,'n'},
    {1,"",		M_ReadThis,'i'},
    {1,"",		M_ChangeAutosave,'a'}
};

menu_t  OptionsDef =
{
    opt_end,
    &MainDef,
    OptionsMenu,
    M_DrawOptions,
    60,37,
    0
};

//
// DISPLAY SETTINGS MENU
//
enum
{
    display_resolution,
    display_fullscreen,
    display_aspect,
    display_integer_scale,
    display_back,
    display_end
} display_e;

menuitem_t DisplayMenu[]=
{
    {2,"", M_DisplayResolution,'r'},
    {2,"", M_DisplayFullscreen,'f'},
    {2,"", M_DisplayAspect,'a'},
    {2,"", M_DisplayScale,'i'},
    {1,"", M_Options,'b'}
};

menu_t DisplayDef =
{
    display_end,
    &OptionsDef,
    DisplayMenu,
    M_DrawDisplay,
    60,37,
    0
};

//
// NETWORK SETTINGS MENU
//
enum
{
    network_latency,
    network_packet_loss,
    network_verbose,
    network_back,
    network_end
} network_e;

menuitem_t NetworkMenu[]=
{
    {2,"", M_NetLatency,'l'},
    {2,"", M_NetPacketLoss,'p'},
    {1,"", M_NetVerbose,'d'},
    {1,"", M_Options,'b'}
};

menu_t NetworkDef =
{
    network_end,
    &OptionsDef,
    NetworkMenu,
    M_DrawNetwork,
    60,37,
    0
};

//
// Read This! MENU 1 & 2
//
enum
{
    rdthsempty1,
    read1_end
} read_e;

menuitem_t ReadMenu1[] =
{
    {1,"",M_ReadThis2,0}
};

menu_t  ReadDef1 =
{
    read1_end,
    &MainDef,
    ReadMenu1,
    M_DrawReadThis1,
    280,185,
    0
};

enum
{
    rdthsempty2,
    read2_end
} read_e2;

menuitem_t ReadMenu2[]=
{
    {1,"",M_FinishReadThis,0}
};

menu_t  ReadDef2 =
{
    read2_end,
    &ReadDef1,
    ReadMenu2,
    M_DrawReadThis2,
    330,175,
    0
};

//
// SOUND VOLUME MENU
//
enum
{
    sfx_vol,
    sfx_empty1,
    music_vol,
    music_backend_item,
    sfx_empty2,
    sound_end
} sound_e;

menuitem_t SoundMenu[]=
{
    {2,"M_SFXVOL",M_SfxVol,'s'},
    {-1,"",0},
    {2,"M_MUSVOL",M_MusicVol,'m'},
    {2,"",M_MusicBackend,'b'},
    {-1,"",0}
};

menu_t  SoundDef =
{
    sound_end,
    &OptionsDef,
    SoundMenu,
    M_DrawSound,
    80,64,
    0
};

//
// LOAD GAME MENU
//
enum
{
    load1,
    load2,
    load3,
    load4,
    load5,
    load6,
    load_end
} load_e;

menuitem_t LoadMenu[]=
{
    {1,"", M_LoadSelect,'1'},
    {1,"", M_LoadSelect,'2'},
    {1,"", M_LoadSelect,'3'},
    {1,"", M_LoadSelect,'4'},
    {1,"", M_LoadSelect,'5'},
    {1,"", M_LoadSelect,'6'}
};

menu_t  LoadDef =
{
    load_end,
    &SinglePlayerDef,
    LoadMenu,
    M_DrawLoad,
    80,54,
    0
};

//
// SAVE GAME MENU
//
menuitem_t SaveMenu[]=
{
    {1,"", M_SaveSelect,'1'},
    {1,"", M_SaveSelect,'2'},
    {1,"", M_SaveSelect,'3'},
    {1,"", M_SaveSelect,'4'},
    {1,"", M_SaveSelect,'5'},
    {1,"", M_SaveSelect,'6'}
};

menu_t  SaveDef =
{
    load_end,
    &SinglePlayerDef,
    SaveMenu,
    M_DrawSave,
    80,54,
    0
};


//
// M_ReadSaveStrings
//  read the strings from the savegame files
//
void M_ReadSaveStrings(void)
{
    int             handle;
    int             count;
    int             i;
    char    name[256];
	
    for (i = 0;i < load_end;i++)
    {
	if (M_CheckParm("-cdrom"))
	    sprintf(name,"c:\\doomdata\\"SAVEGAMENAME"%d.dsg",i);
	else
	    sprintf(name,SAVEGAMENAME"%d.dsg",i);

	handle = open (name, O_RDONLY | 0, 0666);
	if (handle == -1)
	{
	    strcpy(&savegamestrings[i][0],EMPTYSTRING);
	    LoadMenu[i].status = 0;
	    continue;
	}
	count = read (handle, &savegamestrings[i], SAVESTRINGSIZE);
	close (handle);
	LoadMenu[i].status = 1;
    }
}


//
// M_LoadGame & Cie.
//
void M_DrawLoad(void)
{
    int             i;
	
    V_DrawPatchDirect (72,28,0,W_CacheLumpName("M_LOADG",PU_CACHE));
    for (i = 0;i < load_end; i++)
    {
	M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
	M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i]);
    }
}



//
// Draw border for the savegame description
//
void M_DrawSaveLoadBorder(int x,int y)
{
    int             i;
	
    V_DrawPatchDirect (x-8,y+7,0,W_CacheLumpName("M_LSLEFT",PU_CACHE));
	
    for (i = 0;i < 24;i++)
    {
	V_DrawPatchDirect (x,y+7,0,W_CacheLumpName("M_LSCNTR",PU_CACHE));
	x += 8;
    }

    V_DrawPatchDirect (x,y+7,0,W_CacheLumpName("M_LSRGHT",PU_CACHE));
}



//
// User wants to load this game
//
void M_LoadSelect(int choice)
{
    char    name[256];
	
    if (M_CheckParm("-cdrom"))
	sprintf(name,"c:\\doomdata\\"SAVEGAMENAME"%d.dsg",choice);
    else
	sprintf(name,SAVEGAMENAME"%d.dsg",choice);
    G_LoadGame (name);
    M_ClearMenus ();
}

//
// Selected from DOOM menu
//
void M_LoadGame (int choice)
{
    if (netgame)
    {
	M_StartMessage(LOADNET,NULL,false);
	return;
    }
	
    M_SetupNextMenu(&LoadDef);
    M_ReadSaveStrings();
}


//
//  M_SaveGame & Cie.
//
void M_DrawSave(void)
{
    int             i;
	
    V_DrawPatchDirect (72,28,0,W_CacheLumpName("M_SAVEG",PU_CACHE));
    for (i = 0;i < load_end; i++)
    {
	M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
	M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i]);
    }
	
    if (saveStringEnter)
    {
	i = M_StringWidth(savegamestrings[saveSlot]);
	M_WriteText(LoadDef.x + i,LoadDef.y+LINEHEIGHT*saveSlot,"_");
    }
}

//
// M_Responder calls this when user is finished
//
void M_DoSave(int slot)
{
    G_SaveGame (slot,savegamestrings[slot]);
    M_ClearMenus ();

    // PICK QUICKSAVE SLOT YET?
    if (quickSaveSlot == -2)
	quickSaveSlot = slot;
}

//
// User wants to save. Start string input for M_Responder
//
void M_SaveSelect(int choice)
{
    // we are going to be intercepting all chars
    saveStringEnter = 1;
    
    saveSlot = choice;
    strcpy(saveOldString,savegamestrings[choice]);
    if (!strcmp(savegamestrings[choice],EMPTYSTRING))
	savegamestrings[choice][0] = 0;
    saveCharIndex = strlen(savegamestrings[choice]);
}

//
// Selected from DOOM menu
//
void M_SaveGame (int choice)
{
    if (!usergame)
    {
	M_StartMessage(SAVEDEAD,NULL,false);
	return;
    }
	
    if (gamestate != GS_LEVEL)
	return;
	
    M_SetupNextMenu(&SaveDef);
    M_ReadSaveStrings();
}



//
//      M_QuickSave
//
char    tempstring[80];

void M_QuickSaveResponse(int ch)
{
    if (ch == 'y')
    {
	M_DoSave(quickSaveSlot);
	S_StartSound(NULL,sfx_swtchx);
    }
}

void M_QuickSave(void)
{
    if (!usergame)
    {
	S_StartSound(NULL,sfx_oof);
	return;
    }

    if (gamestate != GS_LEVEL)
	return;
	
    if (quickSaveSlot < 0)
    {
	M_StartControlPanel();
	M_ReadSaveStrings();
	M_SetupNextMenu(&SaveDef);
	quickSaveSlot = -2;	// means to pick a slot now
	return;
    }
    sprintf(tempstring,QSPROMPT,savegamestrings[quickSaveSlot]);
    M_StartMessage(tempstring,M_QuickSaveResponse,true);
}



//
// M_QuickLoad
//
void M_QuickLoadResponse(int ch)
{
    if (ch == 'y')
    {
	M_LoadSelect(quickSaveSlot);
	S_StartSound(NULL,sfx_swtchx);
    }
}


void M_QuickLoad(void)
{
    if (netgame)
    {
	M_StartMessage(QLOADNET,NULL,false);
	return;
    }
	
    if (quickSaveSlot < 0)
    {
	M_StartMessage(QSAVESPOT,NULL,false);
	return;
    }
    sprintf(tempstring,QLPROMPT,savegamestrings[quickSaveSlot]);
    M_StartMessage(tempstring,M_QuickLoadResponse,true);
}




//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
void M_DrawReadThis1(void)
{
    inhelpscreens = true;
    switch ( gamemode )
    {
      case commercial:
	V_DrawPatchDirect (0,0,0,W_CacheLumpName("HELP",PU_CACHE));
	break;
      case shareware:
      case registered:
      case retail:
	V_DrawPatchDirect (0,0,0,W_CacheLumpName("HELP1",PU_CACHE));
	break;
      default:
	break;
    }
    return;
}



//
// Read This Menus - optional second page.
//
void M_DrawReadThis2(void)
{
    inhelpscreens = true;
    switch ( gamemode )
    {
      case retail:
      case commercial:
	// This hack keeps us from having to change menus.
	V_DrawPatchDirect (0,0,0,W_CacheLumpName("CREDIT",PU_CACHE));
	break;
      case shareware:
      case registered:
	V_DrawPatchDirect (0,0,0,W_CacheLumpName("HELP2",PU_CACHE));
	break;
      default:
	break;
    }
    return;
}


//
// Change Sfx & Music volumes
//
void M_DrawSound(void)
{
    const char *backend_name = "UNKNOWN";
    V_DrawPatchDirect (60,38,0,W_CacheLumpName("M_SVOL",PU_CACHE));

    M_DrawThermo(SoundDef.x,SoundDef.y+LINEHEIGHT*(sfx_vol+1),
		 15,snd_SfxVolume);

    M_DrawThermo(SoundDef.x,SoundDef.y+LINEHEIGHT*(music_vol+1),
		 15,snd_MusicVolume);

    switch (music_backend)
    {
	case 0:
	    backend_name = "ADLMIDI";
	    break;
	case 1:
	    backend_name = "OPNMIDI";
	    break;
	case 2:
	    backend_name = "ALSA SEQ";
	    break;
	default:
	    break;
    }

    M_WriteText(SoundDef.x, SoundDef.y + LINEHEIGHT*music_backend_item,
		"MUSIC BACKEND");
    M_WriteText(SoundDef.x + 120, SoundDef.y + LINEHEIGHT*music_backend_item,
		(char *)backend_name);
}

void M_Sound(int choice)
{
    M_SetupNextMenu(&SoundDef);
}

void M_SfxVol(int choice)
{
    switch(choice)
    {
      case 0:
	if (snd_SfxVolume)
	    snd_SfxVolume--;
	break;
      case 1:
	if (snd_SfxVolume < 15)
	    snd_SfxVolume++;
	break;
    }
	
    S_SetSfxVolume(snd_SfxVolume /* *8 */);
}

void M_MusicVol(int choice)
{
    switch(choice)
    {
      case 0:
	if (snd_MusicVolume)
	    snd_MusicVolume--;
	break;
      case 1:
	if (snd_MusicVolume < 15)
	    snd_MusicVolume++;
	break;
    }
	
    S_SetMusicVolume(snd_MusicVolume /* *8 */);
}

void M_MusicBackend(int choice)
{
    if (choice)
	music_backend = (music_backend + 1) % 3;
    else
	music_backend = (music_backend + 2) % 3;

    I_InitMusic();
}




//
// M_DrawMainMenu
//
void M_DrawMainMenu(void)
{
    V_DrawPatchDirect (94,2,0,W_CacheLumpName("M_DOOM",PU_CACHE));

    M_WriteText(MainDef.x, MainDef.y + LINEHEIGHT*main_singleplayer, "SINGLE PLAYER");
    M_WriteText(MainDef.x, MainDef.y + LINEHEIGHT*main_multiplayer,  "MULTIPLAYER");
    M_WriteText(MainDef.x, MainDef.y + LINEHEIGHT*main_options,      "OPTIONS");
    M_WriteText(MainDef.x, MainDef.y + LINEHEIGHT*main_quit,         "QUIT GAME");
}

void M_DrawSinglePlayer(void)
{
    int title_x = 160 - M_StringWidth("SINGLE PLAYER")/2;
    M_WriteText(title_x, 15, "SINGLE PLAYER");
    M_WriteText(SinglePlayerDef.x, SinglePlayerDef.y + LINEHEIGHT*sp_newgame, "NEW GAME");
    M_WriteText(SinglePlayerDef.x, SinglePlayerDef.y + LINEHEIGHT*sp_loadgame, "LOAD GAME");
    M_WriteText(SinglePlayerDef.x, SinglePlayerDef.y + LINEHEIGHT*sp_savegame, "SAVE GAME");
    M_WriteText(SinglePlayerDef.x, SinglePlayerDef.y + LINEHEIGHT*sp_back, "BACK");
}

void M_OpenSinglePlayer(int choice)
{
    (void)choice;
    M_SetupNextMenu(&SinglePlayerDef);
}

void M_OpenMainMenu(int choice)
{
    (void)choice;
    M_SetupNextMenu(&MainDef);
}

void M_DrawPauseMenu(void)
{
    int title_x = 160 - M_StringWidth("PAUSE MENU")/2;
    M_WriteText(title_x, 15, "PAUSE MENU");

    // Display player status
    M_WriteText(PauseDef.x, PauseDef.y - 30, "STATUS");

    char status[64];
    player_t *player = &players[consoleplayer];
    int health = player->health;
    int armor = player->armorpoints;

    sprintf(status, "HEALTH: %d%%", health > 100 ? 100 : health);
    M_WriteText(PauseDef.x + 80, PauseDef.y - 30, status);

    sprintf(status, "ARMOR: %d%%", armor > 100 ? 100 : armor);
    M_WriteText(PauseDef.x + 80, PauseDef.y - 20, status);

    // Draw menu items
    M_WriteText(PauseDef.x, PauseDef.y + LINEHEIGHT*pause_status, "VIEW STATUS");
    M_WriteText(PauseDef.x, PauseDef.y + LINEHEIGHT*pause_save, "SAVE GAME");
    M_WriteText(PauseDef.x, PauseDef.y + LINEHEIGHT*pause_load, "LOAD GAME");
    M_WriteText(PauseDef.x, PauseDef.y + LINEHEIGHT*pause_options, "OPTIONS");
    M_WriteText(PauseDef.x, PauseDef.y + LINEHEIGHT*pause_quit, "QUIT GAME");
    M_WriteText(PauseDef.x, PauseDef.y + LINEHEIGHT*pause_back, "RESUME GAME");
}

void M_ViewStatus(int choice)
{
    (void)choice;
    // Open a detailed status screen (can use existing help/read screens as overlay)
    M_StartMessage("GAME STATUS\n\n" "NOT YET IMPLEMENTED\n\nPRESS A KEY", NULL, false);
}

void M_Resume(int choice)
{
    (void)choice;
    M_ClearMenus();
}




//
// M_NewGame
//
void M_DrawNewGame(void)
{
    V_DrawPatchDirect (96,14,0,W_CacheLumpName("M_NEWG",PU_CACHE));
    V_DrawPatchDirect (54,38,0,W_CacheLumpName("M_SKILL",PU_CACHE));
}

void M_NewGame(int choice)
{
    if (netgame && !demoplayback)
    {
	M_StartMessage(NEWGAME,NULL,false);
	return;
    }
	
    if ( gamemode == commercial )
	M_SetupNextMenu(&NewDef);
    else
	M_SetupNextMenu(&EpiDef);
}


//
//      M_Episode
//
int     epi;

//
// M_ScanAvailableMaps
// Scans the WAD file for available maps in a given episode
// Returns the number of available maps found
// For use in dynamic level/map selection systems
//
int M_ScanAvailableMaps(int episode, int* available_maps, int max_maps)
{
    int count = 0;
    int map_num;
    char lump_name[9];

    // For episode-based games (DOOM I), scan E#M# format
    // For commercial (DOOM II), scan MAP## format
    if (gamemode == commercial)
    {
        // DOOM II: scan for MAP01-MAP32 (or more in PWADs)
        for (map_num = 1; map_num <= 99 && count < max_maps; map_num++)
        {
            sprintf(lump_name, "MAP%02d", map_num);
            if (W_CheckNumForName(lump_name) >= 0)
            {
                available_maps[count++] = map_num;
            }
        }
    }
    else
    {
        // Episode-based games: scan E#M# format
        for (map_num = 1; map_num <= 9 && count < max_maps; map_num++)
        {
            sprintf(lump_name, "E%dM%d", episode, map_num);
            if (W_CheckNumForName(lump_name) >= 0)
            {
                available_maps[count++] = map_num;
            }
        }
    }

    return count;
}

void M_DrawEpisode(void)
{
    V_DrawPatchDirect (54,38,0,W_CacheLumpName("M_EPISOD",PU_CACHE));
}

void M_VerifyNightmare(int ch)
{
    if (ch != 'y')
	return;
		
    G_DeferedInitNew(nightmare,epi+1,1);
    M_ClearMenus ();
}

void M_ChooseSkill(int choice)
{
    if (choice == nightmare)
    {
	M_StartMessage(NIGHTMARE,M_VerifyNightmare,true);
	return;
    }
	
    G_DeferedInitNew(choice,epi+1,1);
    M_ClearMenus ();
}

void M_Episode(int choice)
{
    if ( (gamemode == shareware)
	 && choice)
    {
	M_StartMessage(SWSTRING,NULL,false);
        // Show the shareware "Read This" screen, then return to episode select.
        ReadDef1.prevMenu = &EpiDef;
	M_SetupNextMenu(&ReadDef1);
	return;
    }

    // Yet another hack...
    if ( (gamemode == registered)
	 && (choice > 2))
    {
      fprintf( stderr,
	       "M_Episode: 4th episode requires UltimateDOOM\n");
      choice = 0;
    }
	 
    epi = choice;
    M_SetupNextMenu(&NewDef);
}



//
// M_Options
//
char    detailNames[2][9]	= {"M_GDHIGH","M_GDLOW"};
char	msgNames[2][9]		= {"M_MSGOFF","M_MSGON"};


void M_DrawOptions(void)
{
    char option_text[32];
    V_DrawPatchDirect (108,15,0,W_CacheLumpName("M_OPTTTL",PU_CACHE));

    M_WriteText(OptionsDef.x, OptionsDef.y + LINEHEIGHT*endgame, "END GAME");

    M_WriteText(OptionsDef.x, OptionsDef.y + LINEHEIGHT*messages, "MESSAGES");
    sprintf(option_text, "%s", showMessages ? "ON" : "OFF");
    M_WriteText(OptionsDef.x + 140, OptionsDef.y + LINEHEIGHT*messages, option_text);

    M_WriteText(OptionsDef.x, OptionsDef.y + LINEHEIGHT*detail, "DETAIL");
    sprintf(option_text, "%s", detailLevel ? "LOW" : "HIGH");
    M_WriteText(OptionsDef.x + 140, OptionsDef.y + LINEHEIGHT*detail, option_text);

    M_DrawThermo(OptionsDef.x,OptionsDef.y+LINEHEIGHT*(mousesens+1),
		 10,mouseSensitivity);

    M_WriteText(OptionsDef.x, OptionsDef.y + LINEHEIGHT*mousesens, "MOUSE SENS");
    M_WriteText(OptionsDef.x, OptionsDef.y + LINEHEIGHT*soundvol, "AUDIO SETTINGS");
    M_WriteText(OptionsDef.x, OptionsDef.y + LINEHEIGHT*displayopt, "DISPLAY SETTINGS");
    M_WriteText(OptionsDef.x, OptionsDef.y + LINEHEIGHT*networkopt, "NETWORK SETTINGS");
    M_WriteText(OptionsDef.x, OptionsDef.y + LINEHEIGHT*gameinst, "GAME INSTRUCTIONS");

    M_WriteText(OptionsDef.x, OptionsDef.y + LINEHEIGHT*autosave_opt, "AUTOSAVE");
    sprintf(option_text, "%s", autosave_enable ? "ON" : "OFF");
    M_WriteText(OptionsDef.x + 140, OptionsDef.y + LINEHEIGHT*autosave_opt, option_text);
}

void M_DrawDisplay(void)
{
    char value[32];
    int value_x = DisplayDef.x + 140;
    int title_x = 160 - M_StringWidth("DISPLAY SETTINGS")/2;
    int desktop_w = 0;
    int desktop_h = 0;

    I_GetDesktopResolution(&desktop_w, &desktop_h);

    M_WriteText(title_x, 15, "DISPLAY SETTINGS");

    M_WriteText(DisplayDef.x, DisplayDef.y + LINEHEIGHT*display_resolution, "RESOLUTION");
    if (vid_window_width == desktop_w && vid_window_height == desktop_h)
        sprintf(value, "DESKTOP %dx%d", desktop_w, desktop_h);
    else
        sprintf(value, "%dx%d", vid_window_width, vid_window_height);
    M_WriteText(value_x, DisplayDef.y + LINEHEIGHT*display_resolution, value);

    M_WriteText(DisplayDef.x, DisplayDef.y + LINEHEIGHT*display_fullscreen, "FULLSCREEN");
    M_WriteText(value_x, DisplayDef.y + LINEHEIGHT*display_fullscreen,
		vid_fullscreen ? "ON" : "OFF");

    M_WriteText(DisplayDef.x, DisplayDef.y + LINEHEIGHT*display_aspect, "ASPECT");
    M_WriteText(value_x, DisplayDef.y + LINEHEIGHT*display_aspect, (char *)GetAspectLabel());

    M_WriteText(DisplayDef.x, DisplayDef.y + LINEHEIGHT*display_integer_scale, "INTEGER SCALE");
    M_WriteText(value_x, DisplayDef.y + LINEHEIGHT*display_integer_scale,
		vid_integer_scale ? "ON" : "OFF");

    M_WriteText(DisplayDef.x, DisplayDef.y + LINEHEIGHT*display_back, "BACK");
}

void M_DrawNetwork(void)
{
    char value[32];
    int value_x = NetworkDef.x + 140;
    int title_x = 160 - M_StringWidth("NETWORK SETTINGS")/2;

    M_WriteText(title_x, 15, "NETWORK SETTINGS");

    M_WriteText(NetworkDef.x, NetworkDef.y + LINEHEIGHT*network_latency, "LATENCY");
    sprintf(value, "%d MS", I_GetNetLatencyMs());
    M_WriteText(value_x, NetworkDef.y + LINEHEIGHT*network_latency, value);

    M_WriteText(NetworkDef.x, NetworkDef.y + LINEHEIGHT*network_packet_loss, "PACKET LOSS");
    sprintf(value, "%d%%", I_GetNetPacketLoss());
    M_WriteText(value_x, NetworkDef.y + LINEHEIGHT*network_packet_loss, value);

    M_WriteText(NetworkDef.x, NetworkDef.y + LINEHEIGHT*network_verbose, "DETAILS");
    sprintf(value, "%s", mp_verbose_info ? "RICH" : "BASIC");
    M_WriteText(value_x, NetworkDef.y + LINEHEIGHT*network_verbose, value);

    M_WriteText(NetworkDef.x, NetworkDef.y + LINEHEIGHT*network_back, "BACK");
    M_WriteText(NetworkDef.x, NetworkDef.y + LINEHEIGHT*(network_back + 1), "SIMULATION ONLY");
}

void M_Options(int choice)
{
    M_SetupNextMenu(&OptionsDef);
}

void M_OpenDisplay(int choice)
{
    M_SetupNextMenu(&DisplayDef);
}

void M_OpenNetwork(int choice)
{
    M_SetupNextMenu(&NetworkDef);
}

void M_NetVerbose(int choice)
{
    (void)choice;
    mp_verbose_info = !mp_verbose_info;
}

//
// MULTIPLAYER MENUS (Phase 1 UI scaffolding; networking becomes async in Phase 3)
//

void M_LobbyStartGame(int choice);
void M_ManualConnect(int choice);
void M_ManualBack(int choice);
void M_DrawManualConnect(void);
void M_HostSkill(int choice);
void M_HostEpisode(int choice);
void M_HostMap(int choice);

enum
{
    mp_main_host,
    mp_main_join,
    mp_main_back,
    mp_main_end
} mp_main_e;

menuitem_t MultiplayerMenu[]=
{
    {1,"", M_HostSetup,'h'},
    {1,"", M_JoinSetup,'j'},
    {1,"", M_OpenMainMenu,'b'}
};

menu_t MultiplayerDef =
{
    mp_main_end,
    &MainDef,
    MultiplayerMenu,
    M_DrawMultiplayer,
    60,37,
    0
};

enum
{
    host_players,
    host_vanilla,
    host_skill,
    host_episode,
    host_map,
    host_start,
    host_back,
    host_end
} host_e;

menuitem_t HostMenu[]=
{
    {2,"", M_HostPlayers,'p'},
    {2,"", M_HostVanilla,'v'},
    {2,"", M_HostSkill,'k'},
    {2,"", M_HostEpisode,'e'},
    {2,"", M_HostMap,'m'},
    {1,"", M_HostStart,'s'},
    {1,"", M_OpenMultiplayer,'b'}
};

menu_t HostDef =
{
    host_end,
    &MultiplayerDef,
    HostMenu,
    M_DrawHostSetup,
    60,37,
    0
};

enum
{
    join_slot0,
    join_slot1,
    join_slot2,
    join_slot3,
    join_slot4,
    join_slot5,
    join_slot6,
    join_slot7,
    join_manual,
    join_refresh,
    join_back,
    join_end
} join_e;

menuitem_t JoinMenu[]=
{
    {1,"", M_JoinSelect,'1'},
    {1,"", M_JoinSelect,'2'},
    {1,"", M_JoinSelect,'3'},
    {1,"", M_JoinSelect,'4'},
    {1,"", M_JoinSelect,'5'},
    {1,"", M_JoinSelect,'6'},
    {1,"", M_JoinSelect,'7'},
    {1,"", M_JoinSelect,'8'},
    {1,"", M_JoinManual,'m'},
    {1,"", M_JoinRefresh,'r'},
    {1,"", M_OpenMultiplayer,'b'}
};

menu_t JoinDef =
{
    join_end,
    &MultiplayerDef,
    JoinMenu,
    M_DrawJoinBrowser,
    60,37,
    0
};

enum
{
    manual_connect,
    manual_back,
    manual_end
} manual_e;

menuitem_t ManualMenu[]=
{
    {1,"", M_ManualConnect,'c'},
    {1,"", M_ManualBack,'b'}
};

menu_t ManualDef =
{
    manual_end,
    &JoinDef,
    ManualMenu,
    M_DrawManualConnect,
    60,150,
    0
};

enum
{
    lobby_start,
    lobby_cancel,
    lobby_end
} lobby_e;

menuitem_t LobbyMenu[]=
{
    {1,"", M_LobbyStartGame,'s'},
    {1,"", M_OpenMultiplayer,'b'}
};

menu_t LobbyDef =
{
    lobby_end,
    &MultiplayerDef,
    LobbyMenu,
    M_DrawWaitingLobby,
    60,140,
    0
};

void M_OpenMultiplayer(int choice)
{
    (void)choice;
    if (mp_lobby_inflight)
    {
        I_CancelNetworkInit();
        mp_lobby_inflight = 0;
        mp_lobby_launched = 0;
        mp_lobby_last_status = NET_STATUS_INIT;
    }
    M_SetupNextMenu(&MultiplayerDef);
}

void M_HostSetup(int choice)
{
    (void)choice;
    lobby_state.is_host = 1;
    lobby_state.player_count = 2;
    lobby_state.vanilla_only = 0;
    lobby_state.start_skill = 2;
    lobby_state.start_episode = 1;
    lobby_state.start_map = 1;
    M_SetupNextMenu(&HostDef);
}

void M_JoinSetup(int choice)
{
    (void)choice;
    lobby_state.is_host = 0;
    M_JoinRefresh(0);
    M_SetupNextMenu(&JoinDef);
}

void M_HostPlayers(int choice)
{
    // choice: 0 = left, 1 = right
    int pc = lobby_state.player_count ? lobby_state.player_count : 2;
    if (pc < 2) pc = 2;
    if (pc > 4) pc = 4;
    if (choice)
        pc = (pc == 4) ? 2 : pc + 1;
    else
        pc = (pc == 2) ? 4 : pc - 1;
    lobby_state.player_count = pc;
}

void M_HostVanilla(int choice)
{
    (void)choice;
    lobby_state.vanilla_only = !lobby_state.vanilla_only;
    I_SetVanillaOnly(lobby_state.vanilla_only);
}

void M_HostSkill(int choice)
{
    int s = lobby_state.start_skill ? lobby_state.start_skill : 2;
    if (choice)
        s++;
    else
        s--;
    if (s < 1) s = 5;
    if (s > 5) s = 1;
    lobby_state.start_skill = s;
}

void M_HostEpisode(int choice)
{
    // Doom II ignores episodes; keep at 1.
    if (gamemode == commercial)
    {
        lobby_state.start_episode = 1;
        return;
    }

    int e = lobby_state.start_episode ? lobby_state.start_episode : 1;
    if (choice)
        e++;
    else
        e--;
    if (e < 1) e = 4;
    if (e > 4) e = 1;

    // Shareware only supports episode 1.
    if (gamemode == shareware)
        e = 1;

    lobby_state.start_episode = e;
}

void M_HostMap(int choice)
{
    int max_map = (gamemode == commercial) ? 32 : 9;
    int m = lobby_state.start_map ? lobby_state.start_map : 1;
    if (choice)
        m++;
    else
        m--;
    if (m < 1) m = max_map;
    if (m > max_map) m = 1;
    lobby_state.start_map = m;
}

void M_HostStart(int choice)
{
    (void)choice;
    // Start host lobby network init (async; polled from menu ticker).
    I_SetVanillaOnly(lobby_state.vanilla_only);
    I_SetNetStartSettings(lobby_state.start_skill, lobby_state.start_episode, lobby_state.start_map);
    I_InitNetworkAsync(1, lobby_state.player_count);
    mp_lobby_inflight = 1;
    mp_lobby_launched = 0;
    mp_lobby_last_status = NET_STATUS_WAITING;

    // Host can start the game from the lobby.
    LobbyMenu[lobby_start].status = 1;
    M_SetupNextMenu(&LobbyDef);
}

void M_JoinRefresh(int choice)
{
    (void)choice;
    browser_state.server_count = I_RunLanDiscovery(browser_state.servers, 8);
    S_StartSound(NULL,sfx_swtchn);
}

void M_JoinSelect(int choice)
{
    (void)choice;
    int idx = itemOn; // 0..7
    if (idx < 0 || idx >= 8 || idx >= browser_state.server_count)
    {
        M_StartMessage("No server in that slot.", NULL, false);
        return;
    }
    I_SetConnectTarget(browser_state.servers[idx]);
    I_InitNetworkAsync(0, 0);
    mp_lobby_inflight = 1;
    mp_lobby_launched = 0;
    mp_lobby_last_status = NET_STATUS_WAITING;

    // Client can't start; they wait for host START.
    LobbyMenu[lobby_start].status = -1;
    M_SetupNextMenu(&LobbyDef);
}

void M_JoinManual(int choice)
{
    (void)choice;
    mp_manual_ip_enter = 1;
    memset(mp_manual_old_ip, 0, sizeof(mp_manual_old_ip));
    strncpy(mp_manual_old_ip, browser_state.manual_ip, sizeof(mp_manual_old_ip) - 1);
    browser_state.manual_ip_cursor = (int)strlen(browser_state.manual_ip);
    M_SetupNextMenu(&ManualDef);
}

void M_DrawMultiplayer(void)
{
    int title_x = 160 - M_StringWidth("MULTIPLAYER")/2;
    M_WriteText(title_x, 15, "MULTIPLAYER");
    M_WriteText(MultiplayerDef.x, MultiplayerDef.y + LINEHEIGHT*mp_main_host, "HOST GAME");
    M_WriteText(MultiplayerDef.x, MultiplayerDef.y + LINEHEIGHT*mp_main_join, "JOIN GAME");
    M_WriteText(MultiplayerDef.x, MultiplayerDef.y + LINEHEIGHT*mp_main_back, "BACK");
}

void M_DrawHostSetup(void)
{
    char value[32];
    int value_x = HostDef.x + 140;
    int title_x = 160 - M_StringWidth("HOST GAME")/2;
    int pc = lobby_state.player_count ? lobby_state.player_count : 2;

    M_WriteText(title_x, 15, "HOST GAME");
    M_WriteText(HostDef.x, HostDef.y + LINEHEIGHT*host_players, "PLAYERS");
    sprintf(value, "%d", pc);
    M_WriteText(value_x, HostDef.y + LINEHEIGHT*host_players, value);

    M_WriteText(HostDef.x, HostDef.y + LINEHEIGHT*host_vanilla, "VANILLA ONLY");
    M_WriteText(value_x, HostDef.y + LINEHEIGHT*host_vanilla, lobby_state.vanilla_only ? "ON" : "OFF");

    M_WriteText(HostDef.x, HostDef.y + LINEHEIGHT*host_skill, "SKILL");
    sprintf(value, "%d", lobby_state.start_skill ? lobby_state.start_skill : 2);
    M_WriteText(value_x, HostDef.y + LINEHEIGHT*host_skill, value);

    M_WriteText(HostDef.x, HostDef.y + LINEHEIGHT*host_episode, "EPISODE");
    if (gamemode == commercial)
        strcpy(value, "-");
    else
        sprintf(value, "%d", lobby_state.start_episode ? lobby_state.start_episode : 1);
    M_WriteText(value_x, HostDef.y + LINEHEIGHT*host_episode, value);

    M_WriteText(HostDef.x, HostDef.y + LINEHEIGHT*host_map, "MAP");
    if (gamemode == commercial)
        sprintf(value, "%d", lobby_state.start_map ? lobby_state.start_map : 1);
    else
        sprintf(value, "%d", lobby_state.start_map ? lobby_state.start_map : 1);
    M_WriteText(value_x, HostDef.y + LINEHEIGHT*host_map, value);

    M_WriteText(HostDef.x, HostDef.y + LINEHEIGHT*host_start, "START");
    M_WriteText(HostDef.x, HostDef.y + LINEHEIGHT*host_back, "BACK");
}

void M_DrawJoinBrowser(void)
{
    char line[64];
    int title_x = 160 - M_StringWidth("JOIN GAME")/2;
    int y = JoinDef.y;

    M_WriteText(title_x, 15, "JOIN GAME");
    if (browser_state.server_count <= 0)
        M_WriteText(JoinDef.x, y - LINEHEIGHT, "NO SERVERS FOUND");
    else
    {
        char s[32];
        sprintf(s, "SERVERS: %d", browser_state.server_count);
        M_WriteText(JoinDef.x, y - LINEHEIGHT, s);
    }

    for (int i = 0; i < 8; ++i)
    {
        M_DrawSaveLoadBorder(JoinDef.x, y + LINEHEIGHT*i);
        if (i < browser_state.server_count)
        {
            IPaddress a = browser_state.servers[i];
            Uint32 h = SDL_SwapBE32(a.host);
            sprintf(line, "%u.%u.%u.%u:%u",
                    (h >> 24) & 0xff, (h >> 16) & 0xff,
                    (h >> 8) & 0xff, h & 0xff,
                    SDL_SwapBE16(a.port));
        }
        else
        {
            strcpy(line, "-");
        }
        M_WriteText(JoinDef.x, y + LINEHEIGHT*i, line);
    }

    M_WriteText(JoinDef.x, y + LINEHEIGHT*join_manual, "ENTER IP MANUALLY");
    M_WriteText(JoinDef.x, y + LINEHEIGHT*join_refresh, "REFRESH");
    M_WriteText(JoinDef.x, y + LINEHEIGHT*join_back, "BACK");
}

void M_DrawManualConnect(void)
{
    int title_x = 160 - M_StringWidth("DIRECT CONNECT")/2;
    int x = 60;
    int y = 37;
    char view[32];
    int cursor = browser_state.manual_ip_cursor;
    int len = (int)strlen(browser_state.manual_ip);
    int view_w = 24;
    int view_start = 0;
    int view_cursor = 0;

    M_WriteText(title_x, 15, "DIRECT CONNECT");
    M_WriteText(x, y + LINEHEIGHT*0, "ENTER SERVER ADDRESS:");
    M_DrawSaveLoadBorder(x, y + LINEHEIGHT*2);

    if (cursor < 0) cursor = 0;
    if (cursor > len) cursor = len;
    if (len > view_w)
    {
        // Keep the cursor visible inside a 24-char window.
        view_start = cursor - (view_w - 1);
        if (view_start < 0) view_start = 0;
        if (view_start > len - view_w) view_start = len - view_w;
    }
    view_cursor = cursor - view_start;
    if (view_cursor < 0) view_cursor = 0;
    if (view_cursor > view_w) view_cursor = view_w;

    memset(view, 0, sizeof(view));
    strncpy(view, browser_state.manual_ip + view_start, sizeof(view) - 1);
    view[view_w] = '\0';

    M_WriteText(x, y + LINEHEIGHT*2, view);

    if (mp_manual_ip_enter)
    {
        char tmp[32];
        if (view_cursor < 0) view_cursor = 0;
        if (view_cursor > view_w) view_cursor = view_w;
        memset(tmp, 0, sizeof(tmp));
        strncpy(tmp, view, sizeof(tmp) - 1);
        tmp[view_cursor] = '\0';
        M_WriteText(x + M_StringWidth(tmp), y + LINEHEIGHT*2, "_");
    }

    M_WriteText(x, y + LINEHEIGHT*4, "EXAMPLES:");
    M_WriteText(x, y + LINEHEIGHT*5, "192.168.1.100");
    M_WriteText(x, y + LINEHEIGHT*6, "10.0.0.5:5029");
    M_WriteText(x, y + LINEHEIGHT*7, "[::1]:5029");

    M_WriteText(ManualDef.x, ManualDef.y + LINEHEIGHT*manual_connect, "CONNECT");
    M_WriteText(ManualDef.x, ManualDef.y + LINEHEIGHT*manual_back, "BACK");
}

void M_ManualBack(int choice)
{
    (void)choice;
    mp_manual_ip_enter = 0;
    strncpy(browser_state.manual_ip, mp_manual_old_ip, sizeof(browser_state.manual_ip) - 1);
    browser_state.manual_ip[sizeof(browser_state.manual_ip) - 1] = '\0';
    browser_state.manual_ip_cursor = (int)strlen(browser_state.manual_ip);
    M_SetupNextMenu(&JoinDef);
}

void M_ManualConnect(int choice)
{
    (void)choice;
    IPaddress addr;

    if (!browser_state.manual_ip[0])
    {
        M_StartMessage("ENTER AN ADDRESS FIRST\n\nPRESS A KEY", NULL, false);
        S_StartSound(NULL,sfx_oof);
        return;
    }

    if (!I_ResolveNetAddress(browser_state.manual_ip, &addr))
    {
        M_StartMessage("INVALID ADDRESS\n\nPRESS A KEY", NULL, false);
        S_StartSound(NULL,sfx_oof);
        return;
    }

    mp_manual_ip_enter = 0;
    I_SetConnectTarget(addr);
    I_InitNetworkAsync(0, 0);
    mp_lobby_inflight = 1;
    mp_lobby_launched = 0;
    mp_lobby_last_status = NET_STATUS_WAITING;
    LobbyMenu[lobby_start].status = -1;
    M_SetupNextMenu(&LobbyDef);
}

static void FormatHex8(const uint8_t in[16], char out[17])
{
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 8; ++i)
    {
        out[i * 2 + 0] = hex[(in[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[(in[i] >> 0) & 0xF];
    }
    out[16] = 0;
}

void M_DrawWaitingLobby(void)
{
    int title_x = 160 - M_StringWidth("LOBBY")/2;
    int x = 60;
    int y = 37;
    M_WriteText(title_x, 15, "LOBBY");

    // Only the host can start the game from the lobby.
    LobbyMenu[lobby_start].status =
        (lobby_state.is_host && mp_lobby_inflight && I_GetLobbyPlayerCount() >= 2) ? 1 : -1;

    if (!mp_verbose_info)
    {
        char names[MAXPLAYERS][16];
        int total = I_GetTotalPlayers();
        int connected = I_GetLobbyPlayerCount();
        char s[32];
        memset(names, 0, sizeof(names));
        I_GetLobbyRoster(names, MAXPLAYERS);

        if (mp_lobby_last_status == NET_STATUS_READY)
            M_WriteText(x, y + LINEHEIGHT*0, lobby_state.is_host ? "READY. STARTING..." : "STARTING...");
        else
            M_WriteText(x, y + LINEHEIGHT*0, lobby_state.is_host ? "WAITING FOR PLAYERS..." : "CONNECTING...");
        sprintf(s, "PLAYERS: %d/%d", connected, total);
        M_WriteText(x, y + LINEHEIGHT*1, s);

        for (int i = 0; i < total && i < MAXPLAYERS; ++i)
        {
            if (names[i][0])
                M_WriteText(x, y + LINEHEIGHT*(2 + i), names[i]);
        }
    }
    else
    {
        uint8_t key[16] = {0}, hash[16] = {0};
        int vanilla = 0;
        char keyhex[17], hashhex[17];
        char names[MAXPLAYERS][16];
        int total = 0;
        int connected = 0;
        I_GetSessionInfo(key, hash, &vanilla);
        total = I_GetTotalPlayers();
        connected = I_GetLobbyPlayerCount();
        memset(names, 0, sizeof(names));
        I_GetLobbyRoster(names, MAXPLAYERS);
        FormatHex8(key, keyhex);
        FormatHex8(hash, hashhex);
        if (mp_lobby_last_status == NET_STATUS_READY)
            M_WriteText(x, y + LINEHEIGHT*0, lobby_state.is_host ? "READY. STARTING..." : "STARTING...");
        else
            M_WriteText(x, y + LINEHEIGHT*0, lobby_state.is_host ? "HOST SESSION" : "CLIENT SESSION");
        M_WriteText(x, y + LINEHEIGHT*1, "SESSION KEY:");
        M_WriteText(x + 120, y + LINEHEIGHT*1, keyhex);
        M_WriteText(x, y + LINEHEIGHT*2, "CONTENT HASH:");
        M_WriteText(x + 120, y + LINEHEIGHT*2, hashhex);
        M_WriteText(x, y + LINEHEIGHT*3, "VANILLA:");
        M_WriteText(x + 120, y + LINEHEIGHT*3, vanilla ? "YES" : "NO");

        {
            char s[32];
            sprintf(s, "PLAYERS: %d/%d", connected, total);
            M_WriteText(x, y + LINEHEIGHT*4, s);
        }
        for (int i = 0; i < total && i < MAXPLAYERS; ++i)
        {
            if (names[i][0])
                M_WriteText(x, y + LINEHEIGHT*(5 + i), names[i]);
        }
    }

    if (lobby_state.is_host)
        M_WriteText(x, y + LINEHEIGHT*10, "ENTER TO START, ESC/BACK TO CANCEL");
    else
        M_WriteText(x, y + LINEHEIGHT*10, "ESC/BACK TO CANCEL");

    if (lobby_state.is_host)
        M_WriteText(LobbyDef.x, LobbyDef.y + LINEHEIGHT*lobby_start, "START GAME");
    M_WriteText(LobbyDef.x, LobbyDef.y + LINEHEIGHT*lobby_cancel, "BACK");
}

static void M_LaunchMultiplayerGame(void)
{
    // Phase 4 focuses on lobby flow. For now, always start a simple default netgame.
    mp_lobby_inflight = 0;
    mp_lobby_launched = 1;
    menuactive = 0;

    {
        int s = 2, e = 1, m = 1;
        I_GetNetStartSettings(&s, &e, &m);
        if (s < 1) s = 2;
        if (e < 1) e = 1;
        if (m < 1) m = 1;
        startskill = (skill_t)s;
        startepisode = e;
        startmap = m;
    }
    G_DeferedInitNew(startskill, startepisode, startmap);
}

void M_LobbyStartGame(int choice)
{
    (void)choice;
    if (!lobby_state.is_host || !mp_lobby_inflight)
        return;

    // Make sure the network layer uses the host's current start settings.
    I_SetNetStartSettings(lobby_state.start_skill, lobby_state.start_episode, lobby_state.start_map);

    if (I_GetLobbyPlayerCount() < 2)
    {
        M_StartMessage("Need at least 2 players to start.", NULL, false);
        return;
    }

    if (!I_LobbyStartGame())
    {
        M_StartMessage("Failed to start game.", NULL, false);
        return;
    }
}

static void M_TickLobby(void)
{
    if (!mp_lobby_inflight || mp_lobby_launched)
        return;

    mp_lobby_last_status = I_PollNetworkInit();

    if (mp_lobby_last_status == NET_STATUS_REJECTED)
    {
        I_CancelNetworkInit();
        mp_lobby_inflight = 0;
        if (I_WasNetContentMismatch())
            M_StartMessage("MOD MISMATCH\n\nPRESS A KEY", NULL, false);
        else
            M_StartMessage("CONNECTION REFUSED\n\nPRESS A KEY", NULL, false);
        S_StartSound(NULL,sfx_oof);
        M_SetupNextMenu(&JoinDef);
        return;
    }

    if (mp_lobby_last_status == NET_STATUS_TIMEOUT)
    {
        I_CancelNetworkInit();
        mp_lobby_inflight = 0;
        M_StartMessage("CONNECTION TIMEOUT\n\nPRESS A KEY", NULL, false);
        S_StartSound(NULL,sfx_oof);
        M_SetupNextMenu(lobby_state.is_host ? &HostDef : &JoinDef);
        return;
    }

    if (mp_lobby_last_status == NET_STATUS_ERROR)
    {
        I_CancelNetworkInit();
        mp_lobby_inflight = 0;
        M_StartMessage("NETWORK ERROR\n\nPRESS A KEY", NULL, false);
        S_StartSound(NULL,sfx_oof);
        M_SetupNextMenu(lobby_state.is_host ? &HostDef : &JoinDef);
        return;
    }

    if (mp_lobby_last_status == NET_STATUS_READY)
    {
        I_FinishNetworkInit();
        M_LaunchMultiplayerGame();
        return;
    }
}

typedef struct
{
    int width;
    int height;
} resolution_t;

static const resolution_t display_resolutions[] =
{
    {640, 480},
    {800, 600},
    {960, 540},
    {1024, 768},
    {1280, 720},
    {1600, 900},
    {1920, 1080},
    {2560, 1440},
    {3840, 2160}
};

static int FindResolutionIndex(int width, int height)
{
    int i;
    int count = (int)(sizeof(display_resolutions) / sizeof(display_resolutions[0]));
    for (i = 0; i < count; i++)
    {
	if (display_resolutions[i].width == width &&
	    display_resolutions[i].height == height)
	{
	    return i;
	}
    }
    return -1;
}

static const char* GetAspectLabel(void)
{
    switch (vid_aspect)
    {
	case 1:
	    return "16:9";
	case 2:
	    return "STRETCH";
	default:
	    return "4:3";
    }
}

static int stored_windowed_width = 0;
static int stored_windowed_height = 0;

void M_DisplayResolution(int choice)
{
    int count = (int)(sizeof(display_resolutions) / sizeof(display_resolutions[0]));
    int desktop_w = 0;
    int desktop_h = 0;
    int index = FindResolutionIndex(vid_window_width, vid_window_height);
    int total = count + 1;
    int current;

    I_GetDesktopResolution(&desktop_w, &desktop_h);

    if (vid_window_width == desktop_w && vid_window_height == desktop_h)
        current = count;
    else if (index >= 0)
        current = index;
    else
        current = count;

    if (choice)
	current = (current + 1) % total;
    else
	current = (current + total - 1) % total;

    if (current == count)
    {
        vid_window_width = desktop_w;
        vid_window_height = desktop_h;
    }
    else
    {
        vid_window_width = display_resolutions[current].width;
        vid_window_height = display_resolutions[current].height;
    }

    if (!vid_fullscreen)
    {
        stored_windowed_width = vid_window_width;
        stored_windowed_height = vid_window_height;
    }
    I_ApplyVideoSettings();
}

void M_DisplayFullscreen(int choice)
{
    choice = 0;
    if (!vid_fullscreen)
    {
        stored_windowed_width = vid_window_width;
        stored_windowed_height = vid_window_height;
    }
    vid_fullscreen = !vid_fullscreen;
    if (!vid_fullscreen && stored_windowed_width > 0 && stored_windowed_height > 0)
    {
        vid_window_width = stored_windowed_width;
        vid_window_height = stored_windowed_height;
    }
    I_ApplyVideoSettings();
}

void M_DisplayAspect(int choice)
{
    if (choice)
	vid_aspect = (vid_aspect + 1) % 3;
    else
	vid_aspect = (vid_aspect + 2) % 3;
    I_ApplyVideoSettings();
}

void M_DisplayScale(int choice)
{
    choice = 0;
    vid_integer_scale = !vid_integer_scale;
    I_ApplyVideoSettings();
}

void M_NetLatency(int choice)
{
    int latency = I_GetNetLatencyMs();
    int delta = 25;

    if (choice)
	latency += delta;
    else
	latency -= delta;

    I_SetNetLatencyMs(latency);
}

void M_NetPacketLoss(int choice)
{
    int loss = I_GetNetPacketLoss();

    if (choice)
	loss += 1;
    else
	loss -= 1;

    I_SetNetPacketLoss(loss);
}



//
//      Toggle messages on/off
//
void M_ChangeMessages(int choice)
{
    // warning: unused parameter `int choice'
    choice = 0;
    showMessages = 1 - showMessages;

    if (!showMessages)
	players[consoleplayer].message = MSGOFF;
    else
	players[consoleplayer].message = MSGON ;

    message_dontfuckwithme = true;
}

void M_ChangeAutosave(int choice)
{
    // warning: unused parameter `int choice'
    choice = 0;
    autosave_enable = 1 - autosave_enable;
}


//
// M_EndGame
//
void M_EndGameResponse(int ch)
{
    if (ch != 'y')
	return;
		
    currentMenu->lastOn = itemOn;
    M_ClearMenus ();
    D_StartTitle ();
}

void M_EndGame(int choice)
{
    choice = 0;
    if (!usergame)
    {
	S_StartSound(NULL,sfx_oof);
	return;
    }
	
    if (netgame)
    {
	M_StartMessage(NETEND,NULL,false);
	return;
    }
	
    M_StartMessage(ENDGAME,M_EndGameResponse,true);
}




//
// M_ReadThis
//
void M_ReadThis(int choice)
{
    choice = 0;
    // If invoked from the Options menu, return there after closing instructions.
    ReadDef1.prevMenu = &OptionsDef;
    M_SetupNextMenu(&ReadDef1);
}

void M_ReadThis2(int choice)
{
    choice = 0;
    M_SetupNextMenu(&ReadDef2);
}

void M_FinishReadThis(int choice)
{
    menu_t *back;
    choice = 0;
    back = ReadDef1.prevMenu ? ReadDef1.prevMenu : &MainDef;
    M_SetupNextMenu(back);
}




//
// M_QuitDOOM
//
int     quitsounds[8] =
{
    sfx_pldeth,
    sfx_dmpain,
    sfx_popain,
    sfx_slop,
    sfx_telept,
    sfx_posit1,
    sfx_posit3,
    sfx_sgtatk
};

int     quitsounds2[8] =
{
    sfx_vilact,
    sfx_getpow,
    sfx_boscub,
    sfx_slop,
    sfx_skeswg,
    sfx_kntdth,
    sfx_bspact,
    sfx_sgtatk
};



void M_QuitResponse(int ch)
{
    if (ch != 'y')
	return;
    if (!netgame)
    {
	if (gamemode == commercial)
	    S_StartSound(NULL,quitsounds2[(gametic>>2)&7]);
	else
	    S_StartSound(NULL,quitsounds[(gametic>>2)&7]);
	I_WaitVBL(105);
    }
    I_Quit ();
}




void M_QuitDOOM(int choice)
{
  // We pick index 0 which is language sensitive,
  //  or one at random, between 1 and maximum number.
  if (language != english )
    sprintf(endstring,"%s\n\n"DOSY, endmsg[0] );
  else
    sprintf(endstring,"%s\n\n"DOSY, endmsg[ (gametic%(NUM_QUITMESSAGES-2))+1 ]);
  
  M_StartMessage(endstring,M_QuitResponse,true);
}




void M_ChangeSensitivity(int choice)
{
    switch(choice)
    {
      case 0:
	if (mouseSensitivity)
	    mouseSensitivity--;
	break;
      case 1:
	if (mouseSensitivity < 9)
	    mouseSensitivity++;
	break;
    }
}




void M_ChangeDetail(int choice)
{
    choice = 0;
    detailLevel = 1 - detailLevel;

    // FIXME - does not work. Remove anyway?
    fprintf( stderr, "M_ChangeDetail: low detail mode n.a.\n");

    return;
    
    /*R_SetViewSize (screenblocks, detailLevel);

    if (!detailLevel)
	players[consoleplayer].message = DETAILHI;
    else
	players[consoleplayer].message = DETAILLO;*/
}




//
//      Menu Functions
//
void
M_DrawThermo
( int	x,
  int	y,
  int	thermWidth,
  int	thermDot )
{
    int		xx;
    int		i;

    if (thermDot < 0)
	thermDot = 0;
    if (thermDot > thermWidth)
	thermDot = thermWidth;

    xx = x;
    V_DrawPatchDirect (xx,y,0,W_CacheLumpName("M_THERML",PU_CACHE));
    xx += 8;
    for (i=0;i<thermWidth;i++)
    {
	V_DrawPatchDirect (xx,y,0,W_CacheLumpName("M_THERMM",PU_CACHE));
	xx += 8;
    }
    V_DrawPatchDirect (xx,y,0,W_CacheLumpName("M_THERMR",PU_CACHE));

    V_DrawPatchDirect ((x+8) + thermDot*8,y,
		       0,W_CacheLumpName("M_THERMO",PU_CACHE));
}



void
M_DrawEmptyCell
( menu_t*	menu,
  int		item )
{
    V_DrawPatchDirect (menu->x - 10,        menu->y+item*LINEHEIGHT - 1, 0,
		       W_CacheLumpName("M_CELL1",PU_CACHE));
}

void
M_DrawSelCell
( menu_t*	menu,
  int		item )
{
    V_DrawPatchDirect (menu->x - 10,        menu->y+item*LINEHEIGHT - 1, 0,
		       W_CacheLumpName("M_CELL2",PU_CACHE));
}


void
M_StartMessage
( char*		string,
  void*		routine,
  boolean	input )
{
    messageLastMenuActive = menuactive;
    messageToPrint = 1;
    messageString = string;
    messageRoutine = routine;
    messageNeedsInput = input;
    menuactive = true;
    return;
}



void M_StopMessage(void)
{
    menuactive = messageLastMenuActive;
    messageToPrint = 0;
}



//
// Find string width from hu_font chars
//
int M_StringWidth(char* string)
{
    int             i;
    int             w = 0;
    int             c;
	
    for (i = 0;i < strlen(string);i++)
    {
	c = toupper(string[i]) - HU_FONTSTART;
	if (c < 0 || c >= HU_FONTSIZE)
	    w += 4;
	else
	    w += SHORT (hu_font[c]->width);
    }
		
    return w;
}



//
//      Find string height from hu_font chars
//
int M_StringHeight(char* string)
{
    int             i;
    int             h;
    int             height = SHORT(hu_font[0]->height);
	
    h = height;
    for (i = 0;i < strlen(string);i++)
	if (string[i] == '\n')
	    h += height;
		
    return h;
}


//
//      Write a string using the hu_font
//
void
M_WriteText
( int		x,
  int		y,
  char*		string)
{
    int		w;
    char*	ch;
    int		c;
    int		cx;
    int		cy;
		

    ch = string;
    cx = x;
    cy = y;
	
    while(1)
    {
	c = *ch++;
	if (!c)
	    break;
	if (c == '\n')
	{
	    cx = x;
	    cy += 12;
	    continue;
	}
		
	c = toupper(c) - HU_FONTSTART;
	if (c < 0 || c>= HU_FONTSIZE)
	{
	    cx += 4;
	    continue;
	}
		
	w = SHORT (hu_font[c]->width);
	if (cx+w > SCREENWIDTH)
	    break;
	V_DrawPatchDirect(cx, cy, 0, hu_font[c]);
	cx+=w;
    }
}



//
// CONTROL PANEL
//

//
// M_Responder
//
boolean M_Responder (event_t* ev)
{
    int             ch;
    int             i;
    static  int     joywait = 0;
    static  int     mousewait = 0;
    static  int     mousey = 0;
    static  int     lasty = 0;
    static  int     mousex = 0;
    static  int     lastx = 0;
	
    ch = -1;
	
    if (ev->type == ev_joystick && joywait < I_GetTime())
    {
	if (ev->data3 == -1)
	{
	    ch = KEY_UPARROW;
	    joywait = I_GetTime() + 5;
	}
	else if (ev->data3 == 1)
	{
	    ch = KEY_DOWNARROW;
	    joywait = I_GetTime() + 5;
	}
		
	if (ev->data2 == -1)
	{
	    ch = KEY_LEFTARROW;
	    joywait = I_GetTime() + 2;
	}
	else if (ev->data2 == 1)
	{
	    ch = KEY_RIGHTARROW;
	    joywait = I_GetTime() + 2;
	}
		
	if (ev->data1&1)
	{
	    ch = KEY_ENTER;
	    joywait = I_GetTime() + 5;
	}
	if (ev->data1&2)
	{
	    ch = KEY_BACKSPACE;
	    joywait = I_GetTime() + 5;
	}
    }
    else
    {
	if (ev->type == ev_mouse && mousewait < I_GetTime())
	{
	    mousey += ev->data3;
	    if (mousey < lasty-30)
	    {
		ch = KEY_DOWNARROW;
		mousewait = I_GetTime() + 5;
		mousey = lasty -= 30;
	    }
	    else if (mousey > lasty+30)
	    {
		ch = KEY_UPARROW;
		mousewait = I_GetTime() + 5;
		mousey = lasty += 30;
	    }
		
	    mousex += ev->data2;
	    if (mousex < lastx-30)
	    {
		ch = KEY_LEFTARROW;
		mousewait = I_GetTime() + 5;
		mousex = lastx -= 30;
	    }
	    else if (mousex > lastx+30)
	    {
		ch = KEY_RIGHTARROW;
		mousewait = I_GetTime() + 5;
		mousex = lastx += 30;
	    }
		
	    if (ev->data1&1)
	    {
		ch = KEY_ENTER;
		mousewait = I_GetTime() + 15;
	    }
			
	    if (ev->data1&2)
	    {
		ch = KEY_BACKSPACE;
		mousewait = I_GetTime() + 15;
	    }
	}
	else
	    if (ev->type == ev_keydown)
	    {
		ch = ev->data1;
	    }
    }
    
    if (ch == -1)
	return false;

    
    // Save Game string input
    if (saveStringEnter)
    {
	switch(ch)
	{
	  case KEY_BACKSPACE:
	    if (saveCharIndex > 0)
	    {
		saveCharIndex--;
		savegamestrings[saveSlot][saveCharIndex] = 0;
	    }
	    break;
				
	  case KEY_ESCAPE:
	    saveStringEnter = 0;
	    strcpy(&savegamestrings[saveSlot][0],saveOldString);
	    break;
				
	  case KEY_ENTER:
	    saveStringEnter = 0;
	    if (savegamestrings[saveSlot][0])
		M_DoSave(saveSlot);
	    break;
				
	  default:
	    ch = toupper(ch);
	    if (ch != 32)
		if (ch-HU_FONTSTART < 0 || ch-HU_FONTSTART >= HU_FONTSIZE)
		    break;
	    if (ch >= 32 && ch <= 127 &&
		saveCharIndex < SAVESTRINGSIZE-1 &&
		M_StringWidth(savegamestrings[saveSlot]) <
		(SAVESTRINGSIZE-2)*8)
	    {
		savegamestrings[saveSlot][saveCharIndex++] = ch;
		savegamestrings[saveSlot][saveCharIndex] = 0;
	    }
	    break;
	}
	return true;
    }
    
    // Take care of any messages that need input
    if (messageToPrint)
    {
	if (messageNeedsInput == true &&
	    !(ch == ' ' || ch == 'n' || ch == 'y' || ch == KEY_ESCAPE))
	    return false;
		
	menuactive = messageLastMenuActive;
	messageToPrint = 0;
	if (messageRoutine)
	    messageRoutine(ch);
			
	menuactive = false;
	S_StartSound(NULL,sfx_swtchx);
	return true;
    }
	
    if (devparm && ch == KEY_F1)
    {
	G_ScreenShot ();
	return true;
    }
		
    
    // F-Keys
    if (!menuactive)
	switch(ch)
	{
	  case KEY_F1:            // Help key
	    M_StartControlPanel ();

	    // In-game help should return to the main menu if the user backs out.
	    ReadDef1.prevMenu = &MainDef;

	    if ( gamemode == retail )
	      currentMenu = &ReadDef2;
	    else
	      currentMenu = &ReadDef1;
	    
	    itemOn = 0;
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
				
	  case KEY_F2:            // Save
	    M_StartControlPanel();
	    S_StartSound(NULL,sfx_swtchn);
	    M_SaveGame(0);
	    return true;
				
	  case KEY_F3:            // Load
	    M_StartControlPanel();
	    S_StartSound(NULL,sfx_swtchn);
	    M_LoadGame(0);
	    return true;
				
	  case KEY_F4:            // Sound Volume
	    M_StartControlPanel ();
	    currentMenu = &SoundDef;
	    itemOn = sfx_vol;
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
				
	  case KEY_F5:            // Detail toggle
	    M_ChangeDetail(0);
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
				
	  case KEY_F6:            // Quicksave
	    S_StartSound(NULL,sfx_swtchn);
	    M_QuickSave();
	    return true;
				
	  case KEY_F7:            // End game
	    S_StartSound(NULL,sfx_swtchn);
	    M_EndGame(0);
	    return true;
				
	  case KEY_F8:            // Toggle messages
	    M_ChangeMessages(0);
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
				
	  case KEY_F9:            // Quickload
	    S_StartSound(NULL,sfx_swtchn);
	    M_QuickLoad();
	    return true;
				
	  case KEY_F10:           // Quit DOOM
	    S_StartSound(NULL,sfx_swtchn);
	    M_QuitDOOM(0);
	    return true;
				
	  case KEY_F11:           // gamma toggle
	    usegamma++;
	    if (usegamma > 4)
		usegamma = 0;
	    players[consoleplayer].message = gammamsg[usegamma];
	    I_SetPalette (W_CacheLumpName ("PLAYPAL",PU_CACHE));
	    return true;
				
	}

    
    // Pop-up menu?
    if (!menuactive)
    {
	if (ch == KEY_ESCAPE)
	{
	    M_StartControlPanel ();
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
	}
	return false;
    }

    // Manual IP entry (Phase 6). Intercepts text input while allowing UP/DOWN menu nav.
    if (mp_manual_ip_enter && currentMenu == &ManualDef)
    {
	int len = (int)strlen(browser_state.manual_ip);

	if (ch == KEY_ESCAPE)
	{
	    // Cancel edit and restore previous value.
	    mp_manual_ip_enter = 0;
	    strncpy(browser_state.manual_ip, mp_manual_old_ip, sizeof(browser_state.manual_ip) - 1);
	    browser_state.manual_ip[sizeof(browser_state.manual_ip) - 1] = '\0';
	    browser_state.manual_ip_cursor = (int)strlen(browser_state.manual_ip);
	    currentMenu = &JoinDef;
	    itemOn = currentMenu->lastOn;
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
	}

	if (ch == KEY_ENTER)
	{
	    if (itemOn == manual_back)
		M_ManualBack(0);
	    else
		M_ManualConnect(0);
	    S_StartSound(NULL,sfx_pistol);
	    return true;
	}

	if (ch == KEY_BACKSPACE)
	{
	    if (browser_state.manual_ip_cursor > 0 && len > 0)
	    {
		int c = browser_state.manual_ip_cursor;
		memmove(&browser_state.manual_ip[c - 1],
			&browser_state.manual_ip[c],
			(size_t)(len - c + 1));
		browser_state.manual_ip_cursor--;
	    }
	    return true;
	}

	if (ch == KEY_LEFTARROW)
	{
	    if (browser_state.manual_ip_cursor > 0)
		browser_state.manual_ip_cursor--;
	    return true;
	}

	if (ch == KEY_RIGHTARROW)
	{
	    if (browser_state.manual_ip_cursor < len)
		browser_state.manual_ip_cursor++;
	    return true;
	}

	if (ch >= 32 && ch <= 126)
	{
	    // Accept digits, letters (hostnames), and address punctuation.
	    if ((ch >= '0' && ch <= '9') ||
		(ch >= 'a' && ch <= 'z') ||
		(ch >= 'A' && ch <= 'Z') ||
		ch == '.' || ch == ':' || ch == '-' || ch == '[' || ch == ']')
	    {
		if (len < (int)sizeof(browser_state.manual_ip) - 1)
		{
		    int c = browser_state.manual_ip_cursor;
		    memmove(&browser_state.manual_ip[c + 1],
			    &browser_state.manual_ip[c],
			    (size_t)(len - c + 1));
		    browser_state.manual_ip[c] = (char)ch;
		    browser_state.manual_ip_cursor++;
		}
		return true;
	    }
	}
    }

    // Keys usable within menu
    switch (ch)
    {
      case KEY_DOWNARROW:
	do
	{
	    if (itemOn+1 > currentMenu->numitems-1)
		itemOn = 0;
	    else itemOn++;
	    S_StartSound(NULL,sfx_pstop);
	} while(currentMenu->menuitems[itemOn].status==-1);
	return true;
		
      case KEY_UPARROW:
	do
	{
	    if (!itemOn)
		itemOn = currentMenu->numitems-1;
	    else itemOn--;
	    S_StartSound(NULL,sfx_pstop);
	} while(currentMenu->menuitems[itemOn].status==-1);
	return true;

      case KEY_LEFTARROW:
	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status == 2)
	{
	    S_StartSound(NULL,sfx_stnmov);
	    currentMenu->menuitems[itemOn].routine(0);
	}
	return true;
		
      case KEY_RIGHTARROW:
	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status == 2)
	{
	    S_StartSound(NULL,sfx_stnmov);
	    currentMenu->menuitems[itemOn].routine(1);
	}
	return true;

      case KEY_ENTER:
	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status)
	{
	    currentMenu->lastOn = itemOn;
	    if (currentMenu->menuitems[itemOn].status == 2)
	    {
		currentMenu->menuitems[itemOn].routine(1);      // right arrow
		S_StartSound(NULL,sfx_stnmov);
	    }
	    else
	    {
		currentMenu->menuitems[itemOn].routine(itemOn);
		S_StartSound(NULL,sfx_pistol);
	    }
	}
	return true;
		
      case KEY_ESCAPE:
	currentMenu->lastOn = itemOn;
	if (currentMenu == &LobbyDef && mp_lobby_inflight)
	{
	    I_CancelNetworkInit();
	    mp_lobby_inflight = 0;
	    mp_lobby_launched = 0;
	    mp_lobby_last_status = NET_STATUS_INIT;
	}
	M_ClearMenus ();
	S_StartSound(NULL,sfx_swtchx);
	return true;
		
      case KEY_BACKSPACE:
	currentMenu->lastOn = itemOn;
	if (currentMenu->prevMenu)
	{
	    if (currentMenu == &LobbyDef && mp_lobby_inflight)
	    {
		I_CancelNetworkInit();
		mp_lobby_inflight = 0;
		mp_lobby_launched = 0;
		mp_lobby_last_status = NET_STATUS_INIT;
	    }
	    currentMenu = currentMenu->prevMenu;
	    itemOn = currentMenu->lastOn;
	    S_StartSound(NULL,sfx_swtchn);
	}
	return true;
	
      default:
	for (i = itemOn+1;i < currentMenu->numitems;i++)
	    if (currentMenu->menuitems[i].alphaKey == ch)
	    {
		itemOn = i;
		S_StartSound(NULL,sfx_pstop);
		return true;
	    }
	for (i = 0;i <= itemOn;i++)
	    if (currentMenu->menuitems[i].alphaKey == ch)
	    {
		itemOn = i;
		S_StartSound(NULL,sfx_pstop);
		return true;
	    }
	break;
	
    }

    return false;
}



//
// M_StartControlPanel
//
void M_StartControlPanel (void)
{
    // intro might call this repeatedly
    if (menuactive)
	return;

    menuactive = 1;

    // If in gameplay, show pause menu instead of main menu
    extern gamestate_t gamestate;
    if (gamestate == GS_LEVEL && usergame && !demoplayback)
    {
	currentMenu = &PauseDef;
    }
    else
    {
	currentMenu = &MainDef;         // JDC
    }
    itemOn = currentMenu->lastOn;   // JDC
}


//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer (void)
{
    static short	x;
    static short	y;
    short		i;
    short		max;
    char		string[40];
    int			start;

    inhelpscreens = false;

    
    // Horiz. & Vertically center string and print it.
    if (messageToPrint)
    {
	start = 0;
	y = 100 - M_StringHeight(messageString)/2;
	while(*(messageString+start))
	{
	    for (i = 0;i < strlen(messageString+start);i++)
		if (*(messageString+start+i) == '\n')
		{
		    memset(string,0,40);
		    strncpy(string,messageString+start,i);
		    start += i+1;
		    break;
		}
				
	    if (i == strlen(messageString+start))
	    {
		strcpy(string,messageString+start);
		start += i;
	    }
				
	    x = 160 - M_StringWidth(string)/2;
	    M_WriteText(x,y,string);
	    y += SHORT(hu_font[0]->height);
	}
	return;
    }

    if (!menuactive)
	return;

    if (currentMenu->routine)
	currentMenu->routine();         // call Draw routine
    
    // DRAW MENU
    x = currentMenu->x;
    y = currentMenu->y;
    max = currentMenu->numitems;

    for (i=0;i<max;i++)
    {
	if (currentMenu->menuitems[i].name[0])
	    V_DrawPatchDirect (x,y,0,
			       W_CacheLumpName(currentMenu->menuitems[i].name ,PU_CACHE));
	y += LINEHEIGHT;
    }

    
    // DRAW SKULL
    V_DrawPatchDirect(x + SKULLXOFF,currentMenu->y - 5 + itemOn*LINEHEIGHT, 0,
		      W_CacheLumpName(skullName[whichSkull],PU_CACHE));

}


//
// M_ClearMenus
//
void M_ClearMenus (void)
{
    menuactive = 0;
    // if (!netgame && usergame && paused)
    //       sendpause = true;
}




//
// M_SetupNextMenu
//
void M_SetupNextMenu(menu_t *menudef)
{
    currentMenu = menudef;
    itemOn = currentMenu->lastOn;
}


//
// M_Ticker
//
void M_Ticker (void)
{
    if (--skullAnimCounter <= 0)
    {
	whichSkull ^= 1;
	skullAnimCounter = 8;
    }

    // Keep multiplayer lobby responsive while waiting for players / handshake.
    if (menuactive && currentMenu == &LobbyDef && mp_lobby_inflight)
        M_TickLobby();
}


//
// M_Init
//
void M_Init (void)
{
    currentMenu = &MainDef;
    menuactive = 0;
    itemOn = currentMenu->lastOn;
    whichSkull = 0;
    skullAnimCounter = 10;
    messageToPrint = 0;
    messageString = NULL;
    messageLastMenuActive = menuactive;
    quickSaveSlot = -1;

    // Here we could catch other version dependencies,
    //  like HELP1/2, and four episodes.

  
    switch ( gamemode )
    {
      case commercial:
	// DOOM II has only one help page; return to single player after selecting skill.
	NewDef.prevMenu = &SinglePlayerDef;
	ReadDef1.routine = M_DrawReadThis1;
	ReadDef1.x = 330;
	ReadDef1.y = 165;
	ReadMenu1[0].routine = M_FinishReadThis;
	break;
      case shareware:
	// Episode 2 and 3 are handled,
	//  branching to an ad screen.
      case registered:
	// We need to remove the fourth episode.
	EpiDef.numitems--;
	break;
      case retail:
	// We are fine.
      default:
	break;
    }
    
}
