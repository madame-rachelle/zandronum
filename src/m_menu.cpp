// Emacs style mode select	 -*- C++ -*- 
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
//		DOOM selection menu, options, episode etc.
//		Sliders and icons. Kinda widget stuff.
//
//-----------------------------------------------------------------------------

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <zlib.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "doomdef.h"
#include "gstrings.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "w_wad.h"
#include "r_local.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_swap.h"
#include "m_random.h"
#include "s_sound.h"
#include "doomstat.h"
#include "m_menu.h"
#include "v_text.h"
#include "st_stuff.h"
#include "d_gui.h"
#include "version.h"
#include "m_png.h"
#include "templates.h"
#include "lists.h"
#include "gi.h"
#include "p_tick.h"
// [BC] New #includes.
#include "chat.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "network.h"
#include "deathmatch.h"
#include "team.h"
#include "campaign.h"
#include "cooperative.h"
#include "g_level.h"

#include "gl/gl_functions.h"

// MACROS ------------------------------------------------------------------

#define SKULLXOFF			-32
#define SELECTOR_XOFFSET	(-28)
#define SELECTOR_YOFFSET	(-1)

// TYPES -------------------------------------------------------------------

struct FSaveGameNode : public Node
{
	char Title[SAVESTRINGSIZE];
	FString Filename;
	bool bOldVersion;
	bool bMissingWads;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void M_DrawSlider (int x, int y, float min, float max, float cur);
void R_GetPlayerTranslation (int color, FPlayerSkin *skin, BYTE *table);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int  M_StringHeight (const char *string);
void M_ClearMenus ();

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void M_NewGame (int choice);
static void M_Multiplayer (int choice);
static void M_Episode (int choice);
static void M_ChooseSkill (int choice);
static void M_ChooseBotSkill (int choice);
static void M_LoadGame (int choice);
static void M_SaveGame (int choice);
static void M_Options (int choice);
static void M_EndGame (int choice);
static void M_ReadThis (int choice);
static void M_ReadThisMore (int choice);
static void M_QuitDOOM (int choice);
static void M_GameFiles (int choice);
static void M_ClearSaveStuff ();

static void SCClass (int choice);
static void M_ChooseClass (int choice);

static void M_FinishReadThis (int choice);
static void M_QuickSave ();
static void M_QuickLoad ();
static void M_LoadSelect (const FSaveGameNode *file);
static void M_SaveSelect (const FSaveGameNode *file);
static void M_ReadSaveStrings ();
static void M_UnloadSaveStrings ();
static FSaveGameNode *M_RemoveSaveSlot (FSaveGameNode *file);
static void M_ExtractSaveData (const FSaveGameNode *file);
static void M_UnloadSaveData ();
static void M_InsertSaveNode (FSaveGameNode *node);
static bool M_SaveLoadResponder (event_t *ev);
static void M_DeleteSaveResponse (int choice);

static void M_DrawMainMenu ();
static void M_DrawReadThis ();
static void M_DrawNewGame ();
static void M_DrawBotSkill ();
static void M_DrawEpisode ();
static void M_DrawLoad ();
static void M_DrawSave ();
static void DrawClassMenu ();
static void DrawHexenSkillMenu ();
static void M_DrawClassMenu ();

static void M_DrawHereticMainMenu ();
static void M_DrawFiles ();

void M_DrawFrame (int x, int y, int width, int height);
static void M_DrawSaveLoadBorder (int x,int y, int len);
static void M_DrawSaveLoadCommon ();
static void M_SetupNextMenu (oldmenu_t *menudef);
/*static*/ void M_StartMessage (const char *string, void(*routine)(int), bool input);

// [RH] For player setup menu.
void M_PlayerSetup ();
static void M_PlayerSetupTicker ();
static void M_PlayerSetupDrawer ();
static void M_RenderPlayerBackdrop ();
static void M_DrawPlayerBackdrop (int x, int y);
/*
static void M_EditPlayerName (int choice);
static void M_ChangePlayerTeam (int choice);
static void M_PlayerNameChanged (FSaveGameNode *dummy);
static void M_PlayerNameNotChanged ();
static void M_SlidePlayerRed (int choice);
static void M_SlidePlayerGreen (int choice);
static void M_SlidePlayerBlue (int choice);
static void M_ChangeClass (int choice);
static void M_ChangeGender (int choice);
static void M_ChangeSkin (int choice);
static void M_ChangeAutoAim (int choice);
*/
static void PickPlayerClass ();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR (String, playerclass)
EXTERN_CVAR (String, name)
EXTERN_CVAR (Int, team)
EXTERN_CVAR ( String, menu_name )
EXTERN_CVAR( Color, menu_color )
EXTERN_CVAR( String, menu_skin );
EXTERN_CVAR( Int, menu_gender );
EXTERN_CVAR( Int, menu_railcolor );
EXTERN_CVAR( Int, menu_handicap);
EXTERN_CVAR( Float, menu_autoaim );
/*
CVAR( Int, menu_connectiontype, 0, 0 );
CVAR( Int, menu_votecommand, 0, 0 );
CVAR( String, menu_voteparameters, "", 0 );
*/
EXTERN_CVAR (Bool, cl_run)
EXTERN_CVAR( Int, cl_announcer )
extern value_t GenderVals[3];
extern value_t AutoaimVals[7];
extern value_t TrailColorVals[11];
void M_WeaponSetup( void );
void M_AcceptPlayerSetupChanges( void );
void M_UndoPlayerSetupChanges( void );

extern bool		sendpause;
extern int		flagsvar;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

EMenuState		menuactive;
menustack_t		MenuStack[16];
int				MenuStackDepth;
int				skullAnimCounter;	// skull animation counter
bool			drawSkull;			// [RH] don't always draw skull
bool			M_DemoNoPlay;
bool			OptionsActive;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char		tempstring[80];
static char		underscore[2];

static FSaveGameNode *quickSaveSlot;	// NULL = no quicksave slot picked!
static FSaveGameNode *lastSaveSlot;	// Used for highlighting the most recently used slot in the menu
static int 		messageToPrint;			// 1 = message to be printed
static const char *messageString;		// ...and here is the message string!
static EMenuState messageLastMenuActive;
static bool		messageNeedsInput;		// timed message = no input from user
static void	  (*messageRoutine)(int response);
static int		showSharewareMessage;

static int 		genStringEnter;	// we are going to be entering a savegame string
static size_t	genStringLen;	// [RH] Max # of chars that can be entered
static void	  (*genStringEnd)(FSaveGameNode *);
static void	  (*genStringCancel)();
static int 		saveSlot;		// which slot to save in
static size_t	saveCharIndex;	// which char we're editing

static int		LINEHEIGHT;

static char		savegamestring[SAVESTRINGSIZE];
static FString	EndString;

static short	itemOn; 			// menu item skull is on
static short	whichSkull; 		// which skull to draw
int		MenuTime;
static int		InfoType;
static int		InfoTic;

static const char skullName[2][9] = {"M_SKULL1", "M_SKULL2"};	// graphic name of skulls
static const char cursName[8][8] =	// graphic names of Strife menu selector
{
	"M_CURS1", "M_CURS2", "M_CURS3", "M_CURS4", "M_CURS5", "M_CURS6", "M_CURS7", "M_CURS8"
};

static oldmenu_t *currentMenu;		// current menudef
static oldmenu_t *TopLevelMenu;		// The main menu everything hangs off of

static DCanvas	*FireScreen;
static BYTE		FireRemap[256];

static const char		*genders[3] = { "male", "female", "other" };
/*static*/ FPlayerClass	*PlayerClass;
/*static*/ int			PlayerSkin;
/*static*/ FState		*PlayerState;
static int		PlayerTics;
/*static*/ int		PlayerRotation;

static DCanvas			*SavePic;
static FBrokenLines		*SaveComment;
static List				SaveGames;
static FSaveGameNode	*TopSaveGame;
static FSaveGameNode	*SelSaveGame;
static FSaveGameNode	NewSaveNode;

static int 	epi;				// Selected episode

// PRIVATE MENU DEFINITIONS ------------------------------------------------

//
// DOOM MENU
//
static oldmenuitem_t MainMenu[]=
{
	{1,0,'n',"M_NGAME",false,false,M_NewGame},
	{1,0,'m',"M_MULTI",false,false,M_Multiplayer},
	{1,0,'o',"M_OPTION",false,false,M_Options},
	{1,0,'l',"M_LOADG",false,false,M_LoadGame},
	{1,0,'s',"M_SAVEG",false,false,M_SaveGame},
	{1,0,'r',"M_RDTHIS",false,false,M_ReadThis},	// Another hickup with Special edition.
	{1,0,'q',"M_QUITG",false,false,M_QuitDOOM}
};

static oldmenu_t MainDef =
{
	countof(MainMenu),
	MainMenu,
	M_DrawMainMenu,
	97,64,
	0
};

//
// HERETIC MENU
//
static oldmenuitem_t HereticMainMenu[] =
{
	{1,1,'n',"MNU_NEWGAME",false,false,M_NewGame},
	{1,1,'m',"MNU_MULTIPLAYER",false,false,M_Multiplayer},
	{1,1,'o',"MNU_OPTIONS",false,false,M_Options},
	{1,1,'f',"MNU_GAMEFILES",false,false,M_GameFiles},
	{1,1,'i',"MNU_INFO",false,false,M_ReadThis},
	{1,1,'q',"MNU_QUITGAME",false,false,M_QuitDOOM}
};

static oldmenu_t HereticMainDef =
{
	countof(HereticMainMenu),
	HereticMainMenu,
	M_DrawHereticMainMenu,
	110, 56,
	0
};

//
// HEXEN "NEW GAME" MENU
//
static oldmenuitem_t ClassItems[] =
{
	{ 1,1, 'f', "MNU_FIGHTER", false, false, SCClass },
	{ 1,1, 'c', "MNU_CLERIC", false, false, SCClass },
	{ 1,1, 'm', "MNU_MAGE", false, false, SCClass },
	{ 1,1, 'r', "MNU_RANDOM", false, false, SCClass }	// [RH]
};

static oldmenu_t ClassMenu =
{
	4, ClassItems,
	DrawClassMenu,
	66, 58,
	0
};

//
// [GRB] CLASS SELECT
//
oldmenuitem_t ClassMenuItems[8] =
{
	{1,1,0, NULL, false, false, M_ChooseClass },
	{1,1,0, NULL, false, false, M_ChooseClass },
	{1,1,0, NULL, false, false, M_ChooseClass },
	{1,1,0, NULL, false, false, M_ChooseClass },
	{1,1,0, NULL, false, false, M_ChooseClass },
	{1,1,0, NULL, false, false, M_ChooseClass },
	{1,1,0, NULL, false, false, M_ChooseClass },
	{1,1,0, NULL, false, false, M_ChooseClass },
};

oldmenu_t ClassMenuDef =
{
	0,
	ClassMenuItems,
	M_DrawClassMenu,
	48,63,
	0
};

//
// EPISODE SELECT
//
oldmenuitem_t EpisodeMenu[MAX_EPISODES] =
{
	{1,0,0, NULL, false, false, M_Episode},
	{1,0,0, NULL, false, false, M_Episode},
	{1,0,0, NULL, false, false, M_Episode},
	{1,0,0, NULL, false, false, M_Episode},
	{1,0,0, NULL, false, false, M_Episode},
	{1,0,0, NULL, false, false, M_Episode},
	{1,0,0, NULL, false, false, M_Episode},
	{1,0,0, NULL, false, false, M_Episode},
};

char EpisodeMaps[MAX_EPISODES][8];
char EpisodeSkillHeaders[MAX_EPISODES][64];
bool EpisodeNoSkill[MAX_EPISODES];

oldmenu_t EpiDef =
{
	0,
	EpisodeMenu,		// oldmenuitem_t ->
	M_DrawEpisode,		// drawing routine ->
	48,63,				// x,y
	0	 				// lastOn
};

//
// GAME FILES
//
static oldmenuitem_t FilesItems[] =
{
	{1,1,'l',"MNU_LOADGAME",false,false,M_LoadGame},
	{1,1,'s',"MNU_SAVEGAME",false,false,M_SaveGame}
};

static oldmenu_t FilesMenu =
{
	countof(FilesItems),
	FilesItems,
	M_DrawFiles,
	110,60,
	0
};

//
// DOOM SKILL SELECT
//
static oldmenuitem_t NewGameMenu[]=
{
	{1,0,'i',"M_JKILL",false,false,M_ChooseSkill},
	{1,0,'h',"M_ROUGH",false,false,M_ChooseSkill},
	{1,0,'h',"M_HURT",false,false,M_ChooseSkill},
	{1,0,'u',"M_ULTRA",false,false,M_ChooseSkill},
	{1,0,'n',"M_NMARE",false,false,M_ChooseSkill}
};

static oldmenu_t NewDef =
{
	countof(NewGameMenu),
	NewGameMenu,		// oldmenuitem_t ->
	M_DrawNewGame,		// drawing routine ->
	48,63,				// x,y
	2					// lastOn
};

oldmenuitem_t NewBotGameMenu[]=
{
	{1,0,'m',"M_MOMMY",false,false,M_ChooseBotSkill},
	{1,0,'a',"M_PAIN",false,false,M_ChooseBotSkill},
	{1,0,'b',"M_BRING",false,false,M_ChooseBotSkill},
	{1,0,'t',"M_THRIVE",false,false,M_ChooseBotSkill},
	{1,0,'n',"M_BNMARE",false,false,M_ChooseBotSkill}
};

oldmenu_t BotDef =
{
	countof(NewBotGameMenu),
	NewBotGameMenu,
	M_DrawBotSkill,
	48,63,
	2
};

//
// HERETIC SKILL SELECT
//
static oldmenuitem_t HereticSkillItems[] =
{
	{1,1,'t',"MNU_WETNURSE",false,false,M_ChooseSkill},
	{1,1,'y',"MNU_YELLOWBELLIES",false,false,M_ChooseSkill},
	{1,1,'b',"MNU_BRINGEST",false,false,M_ChooseSkill},
	{1,1,'t',"MNU_SMITE",false,false,M_ChooseSkill},
	{1,1,'b',"BLACK PLAGUE POSSESSES THEE",false,false,M_ChooseSkill}
};

static oldmenu_t HereticSkillMenu =
{
	countof(HereticSkillItems),
	HereticSkillItems,
	M_DrawNewGame,
	38, 30,
	2
};

//
// HEXEN SKILL SELECT
//
static oldmenuitem_t HexenSkillItems[] =
{
	{ 1,1, '1', NULL, false, false, M_ChooseSkill },
	{ 1,1, '2', NULL, false, false, M_ChooseSkill },
	{ 1,1, '3', NULL, false, false, M_ChooseSkill },
	{ 1,1, '4', NULL, false, false, M_ChooseSkill },
	{ 1,1, '5', NULL, false, false, M_ChooseSkill }
};

static oldmenu_t HexenSkillMenu =
{
	5, HexenSkillItems,
	DrawHexenSkillMenu,
	120, 44,
	2
};

/*
//
// [RH] Player Setup Menu
//
static oldmenuitem_t PlayerSetupMenu[] =
{
	{ 1,0,'n',NULL,M_EditPlayerName},
	{ 2,0,'t',NULL,M_ChangePlayerTeam},
	{ 2,0,'r',NULL,M_SlidePlayerRed},
	{ 2,0,'g',NULL,M_SlidePlayerGreen},
	{ 2,0,'b',NULL,M_SlidePlayerBlue},
	{ 2,0,'c',NULL,M_ChangeClass},
	{ 2,0,'s',NULL,M_ChangeSkin},
	{ 2,0,'e',NULL,M_ChangeGender},
	{ 2,0,'a',NULL,M_ChangeAutoAim}
};

static oldmenu_t PSetupDef =
{
	countof(PlayerSetupMenu),
	PlayerSetupMenu,
	M_PlayerSetupDrawer,
	48,	47,
	0
};
*/

menuitem_t PlayerSetupItems[] = {
	{ string,	"Name",						&menu_name,				2.0, 0.0, 0.0, NULL  },
	{ redtext,	" ",						NULL,					0.0, 0.0, 0.0, NULL  },
	{ skintype,	"Skin    ",					&menu_skin,				2.0, 0.0, 0.0, NULL	 },
	{ slider,	"Red",						&menu_color,			0.0, 255.0, 1.0, NULL  },
	{ slider,	"Green",					&menu_color,			0.0, 255.0, 1.0, NULL  },
	{ slider,	"Blue",						&menu_color,			0.0, 255.0, 1.0, NULL  },
	{ discrete, "Railgun color",			&menu_railcolor,		11.0, 0.0, 0.0, TrailColorVals },
	{ redtext,	" ",						NULL,					0.0, 0.0, 0.0, NULL  },
	{ redtext,	" ",						NULL,					0.0, 0.0, 0.0, NULL  },
	{ discrete,	"Always Run",				&cl_run,				2.0, 0.0, 0.0, OnOff },
	{ number,	"Handicap",					&menu_handicap,			0.0, 200.0, 5.0, NULL },
	{ discrete,	"Autoaim",					&menu_autoaim,			7.0, 0.0, 0.0, AutoaimVals },
	{ more,		"Weapon setup",				NULL,					0.0, 0.0, 0.0, {(value_t *)M_WeaponSetup} },
	{ redtext,	" ",						NULL,					0.0, 0.0, 0.0, NULL  },
	{ discrete, "Gender",					&menu_gender,			3.0, 0.0, 0.0, GenderVals },
	{ announcer,"Announcer",				&cl_announcer,			0.0, 0.0, 0.0, NULL },
// [RC] Moved switch team to the Multiplayer menu
	{ redtext,	" ",						NULL,					0.0, 0.0, 0.0, NULL  },
	{ more,		"Undo changes",				NULL,					0.0, 0.0, 0.0, {(value_t *)M_UndoPlayerSetupChanges} },
};

menu_t PlayerSetupMenu = {
	"PLAYER SETUP",
	0,
	countof(PlayerSetupItems),
	0,
	PlayerSetupItems,
	0,
	0,
	0,
	M_PlayerSetupDrawer,
	false,
	M_AcceptPlayerSetupChanges,
	MNF_ALIGNLEFT,
};

//
// Read This! MENU 1 & 2
//
static oldmenuitem_t ReadMenu[] =
{
	{1,0,0,NULL,false,false,M_ReadThisMore}
};

static oldmenu_t ReadDef =
{
	1,
	ReadMenu,
	M_DrawReadThis,
	280,185,
	0
};

//
// LOAD GAME MENU
//
static oldmenuitem_t LoadMenu[]=
{
	{1,0,'1',NULL, false,NULL},
	{1,0,'2',NULL, false,NULL},
	{1,0,'3',NULL, false,NULL},
	{1,0,'4',NULL, false,NULL},
	{1,0,'5',NULL, false,NULL},
	{1,0,'6',NULL, false,NULL},
	{1,0,'7',NULL, false,NULL},
	{1,0,'8',NULL, false,NULL},
};

static oldmenu_t LoadDef =
{
	countof(LoadMenu),
	LoadMenu,
	M_DrawLoad,
	80,54,
	0
};

//
// SAVE GAME MENU
//
static oldmenuitem_t SaveMenu[] =
{
	{1,0,'1',NULL, false,NULL},
	{1,0,'2',NULL, false,NULL},
	{1,0,'3',NULL, false,NULL},
	{1,0,'4',NULL, false,NULL},
	{1,0,'5',NULL, false,NULL},
	{1,0,'6',NULL, false,NULL},
	{1,0,'7',NULL, false,NULL},
	{1,0,'8',NULL, false,NULL},
};

static oldmenu_t SaveDef =
{
	countof(SaveMenu),
	SaveMenu,
	M_DrawSave,
	80,54,
	0
};

// CODE --------------------------------------------------------------------

// [RH] Most menus can now be accessed directly
// through console commands.
CCMD (menu_main)
{
	M_StartControlPanel (true);
	M_SetupNextMenu (TopLevelMenu);
}

CCMD (menu_load)
{	// F3
	M_StartControlPanel (true);
	M_LoadGame (0);
}

CCMD (menu_save)
{	// F2
	M_StartControlPanel (true);
	M_SaveGame (0);
}

CCMD (menu_help)
{	// F1
	M_StartControlPanel (true);
	M_ReadThis (0);
}

CCMD (quicksave)
{	// F6
	//M_StartControlPanel (true);
	S_Sound (CHAN_VOICE, "menu/activate", 1, ATTN_NONE);
	M_QuickSave();
}

CCMD (quickload)
{	// F9
	//M_StartControlPanel (true);
	S_Sound (CHAN_VOICE, "menu/activate", 1, ATTN_NONE);
	M_QuickLoad();
}

CCMD (menu_endgame)
{	// F7
	//M_StartControlPanel (true);
	S_Sound (CHAN_VOICE, "menu/activate", 1, ATTN_NONE);
	M_EndGame(0);
}

CCMD (menu_quit)
{	// F10
	//M_StartControlPanel (true);
	S_Sound (CHAN_VOICE, "menu/activate", 1, ATTN_NONE);
	M_QuitDOOM(0);
}

CCMD (menu_game)
{
	M_StartControlPanel (true);
	M_NewGame(0);
}
								
CCMD (menu_options)
{
	M_StartControlPanel (true);
	M_Options(0);
}

CCMD (menu_player)
{
	M_StartControlPanel (true);
	M_PlayerSetup ();
}

CCMD (bumpgamma)
{
	// [RH] Gamma correction tables are now generated
	// on the fly for *any* gamma level.
	// Q: What are reasonable limits to use here?

	float newgamma = Gamma + 0.1;

	if (newgamma > 3.0)
		newgamma = 1.0;

	Gamma = newgamma;
	Printf ("Gamma correction level %g\n", *Gamma);
}

void M_ActivateMenuInput ()
{
	ResetButtonStates ();
	menuactive = MENU_On;
	// Pause sound effects before we play the menu switch sound.
	// That way, it won't be paused.
	P_CheckTickerPaused ();
}

void M_DeactivateMenuInput ()
{
	menuactive = MENU_Off;
}

void M_DrawFiles ()
{
}

void M_GameFiles (int choice)
{
	M_SetupNextMenu (&FilesMenu);
}

//
// M_ReadSaveStrings
//
// Find savegames and read their titles
//
static void M_ReadSaveStrings ()
{
	if (SaveGames.IsEmpty ())
	{
		void *filefirst;
		findstate_t c_file;
		FString filter;

		atterm (M_UnloadSaveStrings);

		filter = G_BuildSaveName ("*.zds", -1);
		filefirst = I_FindFirst (filter.GetChars(), &c_file);
		if (filefirst != ((void *)(-1)))
		{
			do
			{
				// I_FindName only returns the file's name and not its full path
				FString filepath = G_BuildSaveName (I_FindName(&c_file), -1);
				FILE *file = fopen (filepath, "rb");

				if (file != NULL)
				{
					PNGHandle *png;
					char sig[16];
					char title[SAVESTRINGSIZE+1];
					bool oldVer = true;
					bool addIt = false;
					bool missing = false;

					// ZDoom 1.23 betas 21-33 have the savesig first.
					// Earlier versions have the savesig second.
					// Later versions have the savegame encapsulated inside a PNG.
					//
					// Old savegame versions are always added to the menu so
					// the user can easily delete them if desired.

					title[SAVESTRINGSIZE] = 0;

					if (NULL != (png = M_VerifyPNG (file)))
					{
						char *ver = M_GetPNGText (png, "ZDoom Save Version");
						char *engine = M_GetPNGText (png, "Engine");
						if (ver != NULL)
						{
							if (!M_GetPNGText (png, "Title", title, SAVESTRINGSIZE))
							{
								strncpy (title, I_FindName(&c_file), SAVESTRINGSIZE);
							}
							if (strncmp (ver, SAVESIG, 9) == 0 &&
								atoi (ver+9) >= MINSAVEVER &&
								engine != NULL)
							{
								// Was saved with a compatible ZDoom version,
								// so check if it's for the current game.
								// If it is, add it. Otherwise, ignore it.
								char *iwad = M_GetPNGText (png, "Game WAD");
								if (iwad != NULL)
								{
									if (stricmp (iwad, Wads.GetWadName (FWadCollection::IWAD_FILENUM)) == 0)
									{
										addIt = true;
										oldVer = false;
										missing = !G_CheckSaveGameWads (png, false);
									}
									delete[] iwad;
								}
							}
							else
							{ // An old version
								addIt = true;
							}
							delete[] ver;
						}
						if (engine != NULL)
						{
							delete[] engine;
						}
						delete png;
					}
					else
					{
						fseek (file, 0, SEEK_SET);
						if (fread (sig, 1, 16, file) == 16)
						{

							if (strncmp (sig, "ZDOOMSAVE", 9) == 0)
							{
								if (fread (title, 1, SAVESTRINGSIZE, file) == SAVESTRINGSIZE)
								{
									addIt = true;
								}
							}
							else
							{
								memcpy (title, sig, 16);
								if (fread (title + 16, 1, SAVESTRINGSIZE-16, file) == SAVESTRINGSIZE-16 &&
									fread (sig, 1, 16, file) == 16 &&
									strncmp (sig, "ZDOOMSAVE", 9) == 0)
								{
									addIt = true;
								}
							}
						}
					}

					if (addIt)
					{
						FSaveGameNode *node = new FSaveGameNode;
						node->Filename = filepath;
						node->bOldVersion = oldVer;
						node->bMissingWads = missing;
						memcpy (node->Title, title, SAVESTRINGSIZE);
						M_InsertSaveNode (node);
					}
					fclose (file);
				}
			} while (I_FindNext (filefirst, &c_file) == 0);
			I_FindClose (filefirst);
		}
	}
	if (SelSaveGame == NULL || SelSaveGame->Succ == NULL)
	{
		SelSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
	}
}

static void M_UnloadSaveStrings()
{
	M_UnloadSaveData();
	while (!SaveGames.IsEmpty())
	{
		M_RemoveSaveSlot (static_cast<FSaveGameNode *>(SaveGames.Head));
	}
}

static FSaveGameNode *M_RemoveSaveSlot (FSaveGameNode *file)
{
	FSaveGameNode *next = static_cast<FSaveGameNode *>(file->Succ);

	if (file == TopSaveGame)
	{
		TopSaveGame = next;
	}
	if (quickSaveSlot == file)
	{
		quickSaveSlot = NULL;
	}
	if (lastSaveSlot == file)
	{
		lastSaveSlot = NULL;
	}
	file->Remove ();
	delete file;
	return next;
}

void M_InsertSaveNode (FSaveGameNode *node)
{
	FSaveGameNode *probe;

	if (SaveGames.IsEmpty ())
	{
		SaveGames.AddHead (node);
		return;
	}

	if (node->bOldVersion)
	{ // Add node at bottom of list
		probe = static_cast<FSaveGameNode *>(SaveGames.TailPred);
		while (probe->Pred != NULL && probe->bOldVersion &&
			stricmp (node->Title, probe->Title) < 0)
		{
			probe = static_cast<FSaveGameNode *>(probe->Pred);
		}
		node->Insert (probe);
	}
	else
	{ // Add node at top of list
		probe = static_cast<FSaveGameNode *>(SaveGames.Head);
		while (probe->Succ != NULL && !probe->bOldVersion &&
			stricmp (node->Title, probe->Title) > 0)
		{
			probe = static_cast<FSaveGameNode *>(probe->Succ);
		}
		node->InsertBefore (probe);
	}
}

void M_NotifyNewSave (const char *file, const char *title, bool okForQuicksave)
{
	FSaveGameNode *node;

	if (file == NULL)
		return;

	M_ReadSaveStrings ();

	// See if the file is already in our list
	for (node = static_cast<FSaveGameNode *>(SaveGames.Head);
		 node->Succ != NULL;
		 node = static_cast<FSaveGameNode *>(node->Succ))
	{
#ifdef unix
		if (node->Filename.Compare (file) == 0)
#else
		if (node->Filename.CompareNoCase (file) == 0)
#endif
		{
			strcpy (node->Title, title);
			node->bOldVersion = false;
			node->bMissingWads = false;
			break;
		}
	}

	if (node->Succ == NULL)
	{
		node = new FSaveGameNode;
		strcpy (node->Title, title);
		node->Filename = file;
		node->bOldVersion = false;
		node->bMissingWads = false;
		M_InsertSaveNode (node);
		SelSaveGame = node;
	}

	if (okForQuicksave)
	{
		if (quickSaveSlot == NULL) quickSaveSlot = node;
		lastSaveSlot = node;
	}
}

//
// M_LoadGame & Cie.
//
void M_DrawLoad (void)
{
	if (gameinfo.gametype & (GAME_Doom|GAME_Strife))
	{
		FTexture *title = TexMan["M_LOADG"];
		screen->DrawTexture (title,
			(SCREENWIDTH-title->GetWidth()*CleanXfac)/2, 20*CleanYfac,
			DTA_CleanNoMove, true, TAG_DONE);
	}
	else
	{
		const char *loadgame = GStrings("MNU_LOADGAME");
		screen->DrawText (CR_UNTRANSLATED,
			(SCREENWIDTH - BigFont->StringWidth (loadgame)*CleanXfac)/2, 10*CleanYfac,
			loadgame, DTA_CleanNoMove, true, TAG_DONE);
	}
	screen->SetFont (SmallFont);
	M_DrawSaveLoadCommon ();
}



//
// Draw border for the savegame description
// [RH] Width of the border is variable
//
void M_DrawSaveLoadBorder (int x, int y, int len)
{
	if (gameinfo.gametype & (GAME_Doom|GAME_Strife))
	{
		int i;

		screen->DrawTexture (TexMan["M_LSLEFT"], x-8, y+7, DTA_Clean, true, TAG_DONE);

		for (i = 0; i < len; i++)
		{
			screen->DrawTexture (TexMan["M_LSCNTR"], x, y+7, DTA_Clean, true, TAG_DONE);
			x += 8;
		}

		screen->DrawTexture (TexMan["M_LSRGHT"], x, y+7, DTA_Clean, true, TAG_DONE);
	}
	else
	{
		screen->DrawTexture (TexMan["M_FSLOT"], x, y+1, DTA_Clean, true, TAG_DONE);
	}
}

static void M_ExtractSaveData (const FSaveGameNode *node)
{
	FILE *file;
	PNGHandle *png;

	M_UnloadSaveData ();

	// When breaking comment strings below, be sure to get the spacing from
	// the small font instead of some other font.
	screen->SetFont (SmallFont);

	if (node != NULL &&
		node->Succ != NULL &&
		!node->Filename.IsEmpty() &&
		!node->bOldVersion &&
		(file = fopen (node->Filename.GetChars(), "rb")) != NULL)
	{
		if (NULL != (png = M_VerifyPNG (file)))
		{
			char *time, *pcomment, *comment;
			size_t commentlen, totallen, timelen;

			// Extract comment
			time = M_GetPNGText (png, "Creation Time");
			pcomment = M_GetPNGText (png, "Comment");
			if (pcomment != NULL)
			{
				commentlen = strlen (pcomment);
			}
			else
			{
				commentlen = 0;
			}
			if (time != NULL)
			{
				timelen = strlen (time);
				totallen = timelen + commentlen + 3;
			}
			else
			{
				timelen = 0;
				totallen = commentlen + 1;
			}
			if (totallen != 0)
			{
				comment = new char[totallen];

				if (timelen)
				{
					memcpy (comment, time, timelen);
					comment[timelen] = '\n';
					comment[timelen+1] = '\n';
					timelen += 2;
				}
				if (commentlen)
				{
					memcpy (comment + timelen, pcomment, commentlen);
				}
				comment[timelen+commentlen] = 0;
				SaveComment = V_BreakLines (screen->Font, 216*screen->GetWidth()/640/CleanXfac, comment);
				delete[] comment;
				delete[] time;
				delete[] pcomment;
			}

			// Extract pic
			SavePic = M_CreateCanvasFromPNG (png);

			delete png;
		}
		fclose (file);
	}
}

static void M_UnloadSaveData ()
{
	if (SavePic != NULL)
	{
		delete SavePic;
	}
	if (SaveComment != NULL)
	{
		V_FreeBrokenLines (SaveComment);
	}

	SavePic = NULL;
	SaveComment = NULL;
}

static void M_DrawSaveLoadCommon ()
{
	const int savepicLeft = 10;
	const int savepicTop = 54*CleanYfac;
	const int savepicWidth = 216*screen->GetWidth()/640;
	const int savepicHeight = 135*screen->GetHeight()/400;

	const int rowHeight = (SmallFont->GetHeight() + 1) * CleanYfac;
	const int listboxLeft = savepicLeft + savepicWidth + 14;
	const int listboxTop = savepicTop;
	const int listboxWidth = screen->GetWidth() - listboxLeft - 10;
	const int listboxHeight1 = screen->GetHeight() - listboxTop - 10;
	const int listboxRows = (listboxHeight1 - 1) / rowHeight;
	const int listboxHeight = listboxRows * rowHeight + 1;
	const int listboxRight = listboxLeft + listboxWidth;
	const int listboxBottom = listboxTop + listboxHeight;

	const int commentLeft = savepicLeft;
	const int commentTop = savepicTop + savepicHeight + 16;
	const int commentWidth = savepicWidth;
	const int commentHeight = (51+(screen->GetHeight()>200?10:0))*CleanYfac;
	const int commentRight = commentLeft + commentWidth;
	const int commentBottom = commentTop + commentHeight;

	FSaveGameNode *node;
	int i;
	bool didSeeSelected = false;

	// Draw picture area
	if (gameaction == ga_loadgame || gameaction == ga_savegame)
	{
		return;
	}

	M_DrawFrame (savepicLeft, savepicTop, savepicWidth, savepicHeight);
	if (SavePic != NULL)
	{
		if (currentrenderer==0)
		{
			// The most important question here is:
			// Why the fuck is it necessary to do this in a way that
			// reqires making this distinction here and not by virtually
			// calling an appropriate Blit method for the destination?
			// 
			// Making Blit part of the source buffer is just stupid!
			SavePic->Blit (0, 0, SavePic->GetWidth(), SavePic->GetHeight(),
				screen, savepicLeft, savepicTop, savepicWidth, savepicHeight);
		}
		else
		{
			gl_DrawSavePic(SavePic, SelSaveGame->Filename.GetChars(), savepicLeft, savepicTop, savepicWidth, savepicHeight);
		}
	}
	else
	{
		screen->Clear (savepicLeft, savepicTop,
			savepicLeft+savepicWidth, savepicTop+savepicHeight, 0);

		if (!SaveGames.IsEmpty ())
		{
			const char *text =
				(SelSaveGame == NULL || !SelSaveGame->bOldVersion)
				? GStrings("MNU_NOPICTURE") : GStrings("MNU_DIFFVERSION");
			const int textlen = SmallFont->StringWidth (text)*CleanXfac;

			screen->DrawText (CR_GOLD, savepicLeft+(savepicWidth-textlen)/2,
				savepicTop+(savepicHeight-rowHeight)/2, text,
				DTA_CleanNoMove, true, TAG_DONE);
		}
	}

	// Draw comment area
	M_DrawFrame (commentLeft, commentTop, commentWidth, commentHeight);
	screen->Clear (commentLeft, commentTop, commentRight, commentBottom, 0);
	if (SaveComment != NULL)
	{
		// I'm not sure why SaveComment would go NULL in this loop, but I got
		// a crash report where it was NULL when i reached 1, so now I check
		// for that.
		for (i = 0; SaveComment != NULL && SaveComment[i].Width >= 0 && i < 6; ++i)
		{
			screen->DrawText (CR_GOLD, commentLeft, commentTop
				+ SmallFont->GetHeight()*i*CleanYfac, SaveComment[i].Text,
				DTA_CleanNoMove, true, TAG_DONE);
		}
	}

	// Draw file area
	do
	{
		M_DrawFrame (listboxLeft, listboxTop, listboxWidth, listboxHeight);
		screen->Clear (listboxLeft, listboxTop, listboxRight, listboxBottom, 0);

		if (SaveGames.IsEmpty ())
		{
			const char * text = GStrings("MNU_NOFILES");
			const int textlen = SmallFont->StringWidth (text)*CleanXfac;

			screen->DrawText (CR_GOLD, listboxLeft+(listboxWidth-textlen)/2,
				listboxTop+(listboxHeight-rowHeight)/2, text,
				DTA_CleanNoMove, true, TAG_DONE);
			return;
		}

		for (i = 0, node = TopSaveGame;
			 i < listboxRows && node->Succ != NULL;
			 ++i, node = static_cast<FSaveGameNode *>(node->Succ))
		{
			int color;
			if (node->bOldVersion)
			{
				color = CR_BLUE;
			}
			else if (node->bMissingWads)
			{
				color = CR_ORANGE;
			}
			else if (node == SelSaveGame)
			{
				color = CR_WHITE;
			}
			else
			{
				color = CR_TAN;
			}
			if (node == SelSaveGame)
			{
				screen->Clear (listboxLeft, listboxTop+rowHeight*i,
					listboxRight, listboxTop+rowHeight*(i+1),
					ColorMatcher.Pick (genStringEnter ? 255 : 0, 0, genStringEnter ? 0 : 255));
				didSeeSelected = true;
				if (!genStringEnter)
				{
					screen->DrawText (
						color, listboxLeft+1,
						listboxTop+rowHeight*i+CleanYfac, node->Title,
						DTA_CleanNoMove, true, TAG_DONE);
				}
				else
				{
					screen->DrawText (CR_WHITE, listboxLeft+1,
						listboxTop+rowHeight*i+CleanYfac, savegamestring,
						DTA_CleanNoMove, true, TAG_DONE);
					screen->DrawText (CR_WHITE,
						listboxLeft+1+SmallFont->StringWidth (savegamestring)*CleanXfac,
						listboxTop+rowHeight*i+CleanYfac, underscore,
						DTA_CleanNoMove, true, TAG_DONE);
				}
			}
			else
			{
				screen->DrawText (
					color, listboxLeft+1,
					listboxTop+rowHeight*i+CleanYfac, node->Title,
					DTA_CleanNoMove, true, TAG_DONE);
			}
		}

		// This is dumb: If the selected node was not visible,
		// scroll down and redraw. M_SaveLoadResponder()
		// guarantees that if the node is not visible, it will
		// always be below the visible list instead of above it.
		// This should not really be done here, but I don't care.

		if (!didSeeSelected)
		{
			for (i = 1; node->Succ != NULL && node != SelSaveGame; ++i)
			{
				node = static_cast<FSaveGameNode *>(node->Succ);
			}
			if (node->Succ == NULL)
			{ // SelSaveGame is invalid
				didSeeSelected = true;
			}
			else
			{
				do
				{
					TopSaveGame = static_cast<FSaveGameNode *>(TopSaveGame->Succ);
				} while (--i);
			}
		}
	} while (!didSeeSelected);
}

// Draw a frame around the specified area using the view border
// frame graphics. The border is drawn outside the area, not in it.
void M_DrawFrame (int left, int top, int width, int height)
{
	FTexture *p1, *p2;
	const gameborder_t *border = gameinfo.border;
	int offset = border->offset;
	int size = border->size;
	int x, y;

	// Draw top and bottom sides.
	p1 = TexMan[border->t];
	p2 = TexMan[border->b];
	for (x = left; x < left + width; x += size)
	{
		if (x + size > left + width)
			x = left + width - size;
		screen->DrawTexture (p1, x, top - offset, TAG_DONE);
		screen->DrawTexture (p2, x, top + height, TAG_DONE);
	}

	// Draw left and right sides.
	p1 = TexMan[border->l];
	p2 = TexMan[border->r];
	for (y = top; y < top + height; y += size)
	{
		if (y + size > top + height)
			y = top + height - size;
		screen->DrawTexture (p1, left - offset, y, TAG_DONE);
		screen->DrawTexture (p2, left + width, y, TAG_DONE);
	}

	// Draw beveled corners.
	screen->DrawTexture (TexMan[border->tl], left-offset, top-offset, TAG_DONE);
	screen->DrawTexture (TexMan[border->tr], left+width, top-offset, TAG_DONE);
	screen->DrawTexture (TexMan[border->bl], left-offset, top+height, TAG_DONE);
	screen->DrawTexture (TexMan[border->br], left+width, top+height, TAG_DONE);
}

//
// Selected from DOOM menu
//
void M_LoadGame (int choice)
{
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		M_StartMessage (GStrings("LOADNET"), NULL, false);
		return;
	}
		
	M_SetupNextMenu (&LoadDef);
	drawSkull = false;
	M_ReadSaveStrings ();
	TopSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
	M_ExtractSaveData (SelSaveGame);
}


//
//	M_SaveGame & Cie.
//
void M_DrawSave()
{
	if (gameinfo.gametype & (GAME_Doom|GAME_Strife))
	{
		FTexture *title = TexMan["M_SAVEG"];
		screen->DrawTexture (title,
			(SCREENWIDTH-title->GetWidth()*CleanXfac)/2, 20*CleanYfac,
			DTA_CleanNoMove, true, TAG_DONE);
	}
	else
	{
		const char * text = GStrings("MNU_SAVEGAME");
		screen->DrawText (CR_UNTRANSLATED,
			(SCREENWIDTH - BigFont->StringWidth (text)*CleanXfac)/2, 10*CleanYfac,
			text, DTA_CleanNoMove, true, TAG_DONE);
	}
	screen->SetFont (SmallFont);
	M_DrawSaveLoadCommon ();
}

//
// M_Responder calls this when the user is finished
//
void M_DoSave (FSaveGameNode *node)
{
	if (node != &NewSaveNode)
	{
		G_SaveGame (node->Filename.GetChars(), savegamestring);
	}
	else
	{
		// Find an unused filename and save as that
		FString filename;
		int i;
		FILE *test;

		for (i = 0;; ++i)
		{
			filename = G_BuildSaveName ("save", i);
			test = fopen (filename, "rb");
			if (test == NULL)
			{
				break;
			}
			fclose (test);
		}
		G_SaveGame (filename, savegamestring);
	}
	M_ClearMenus ();
	BorderNeedRefresh = screen->GetPageCount ();
}

//
// Selected from DOOM menu
//
void M_SaveGame (int choice)
{
	if (!usergame || (players[consoleplayer].health <= 0 && NETWORK_GetState( ) == NETSTATE_SINGLE ))
	{
		M_StartMessage (GStrings("SAVEDEAD"), NULL, false);
		return;
	}

	if (gamestate != GS_LEVEL)
		return;

	M_SetupNextMenu(&SaveDef);
	drawSkull = false;

	M_ReadSaveStrings();
	SaveGames.AddHead (&NewSaveNode);
	TopSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
	if (lastSaveSlot == NULL)
	{
		SelSaveGame = &NewSaveNode;
	}
	else
	{
		SelSaveGame = lastSaveSlot;
	}
	M_ExtractSaveData (SelSaveGame);
}



//
//		M_QuickSave
//
void M_QuickSaveResponse (int ch)
{
	if (ch == 'y')
	{
		M_DoSave (quickSaveSlot);
		S_Sound (CHAN_VOICE, "menu/dismiss", 1, ATTN_NONE);
	}
}

void M_QuickSave ()
{
	if (!usergame || (players[consoleplayer].health <= 0 && NETWORK_GetState( ) == NETSTATE_SINGLE ))
	{
		S_Sound (CHAN_VOICE, "menu/invalid", 1, ATTN_NONE);
		return;
	}

	if (gamestate != GS_LEVEL)
		return;
		
	if (quickSaveSlot == NULL)
	{
		M_StartControlPanel(false);
		M_SaveGame (0);
		return;
	}
	sprintf (tempstring, GStrings("QSPROMPT"), quickSaveSlot->Title);
	strcpy (savegamestring, quickSaveSlot->Title);
	M_StartMessage (tempstring, M_QuickSaveResponse, true);
}



//
// M_QuickLoad
//
void M_QuickLoadResponse (int ch)
{
	if (ch == 'y')
	{
		M_LoadSelect (quickSaveSlot);
		S_Sound (CHAN_VOICE, "menu/dismiss", 1, ATTN_NONE);
	}
}


void M_QuickLoad ()
{
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		M_StartMessage (GStrings("QLOADNET"), NULL, false);
		return;
	}
		
	if (quickSaveSlot == NULL)
	{
		M_StartControlPanel(false);
		// signal that whatever gets loaded should be the new quicksave
		quickSaveSlot = (FSaveGameNode *)1;
		M_LoadGame (0);
		return;
	}
	sprintf (tempstring, GStrings("QLPROMPT"), quickSaveSlot->Title);
	M_StartMessage (tempstring, M_QuickLoadResponse, true);
}

//
// Read This Menus
//
void M_DrawReadThis ()
{
	FTexture *tex, *prevpic = NULL;
	fixed_t alpha;

	if (gameinfo.flags & GI_INFOINDEXED)
	{
		char name[9];
		name[8] = 0;
		Wads.GetLumpName (name, Wads.GetNumForName (gameinfo.info.indexed.basePage, ns_graphics) + InfoType);
		tex = TexMan[name];
		if (InfoType > 1)
		{
			Wads.GetLumpName (name, Wads.GetNumForName (gameinfo.info.indexed.basePage, ns_graphics) + InfoType - 1);
			prevpic = TexMan[name];
		}
	}
	else
	{
		tex = TexMan[gameinfo.info.infoPage[InfoType-1]];

		/*====================
		Game-Sensitive F1 Help
		====================*/
		if(( gamestate == GS_LEVEL ) || (gamestate == GS_INTERMISSION ))
		{
			if ( TexMan.CheckForTexture( GAMEMODE_GetF1Texture( GAMEMODE_GetCurrentMode( )), 0, 0 ) -1 )
				tex = TexMan[GAMEMODE_GetF1Texture( GAMEMODE_GetCurrentMode( ))];

			// Custom F1 screens via MAPINFO
			if((level.f1 != NULL) && (strcmp(level.f1, "") != 0))
			{
				if(TexMan.CheckForTexture(level.f1,0,0) == -1)
					TexMan.AddPatch(level.f1); // Needs to be marked as a patch.
				tex = TexMan[level.f1];
			}
		}
		if (InfoType > 1)
		{
			prevpic = TexMan[gameinfo.info.infoPage[InfoType-2]];
		}
	}
	alpha = MIN<fixed_t> (Scale (gametic - InfoTic, OPAQUE, TICRATE/3), OPAQUE);
	if (alpha < OPAQUE && prevpic != NULL)
	{
		screen->DrawTexture (prevpic, 0, 0,
			DTA_DestWidth, screen->GetWidth(),
			DTA_DestHeight, screen->GetHeight(),
			TAG_DONE);
	}
	screen->DrawTexture (tex, 0, 0,
		DTA_DestWidth, screen->GetWidth(),
		DTA_DestHeight, screen->GetHeight(),
		DTA_Alpha, alpha,
		TAG_DONE);
}

//
// M_DrawMainMenu
//
void M_DrawMainMenu (void)
{
	if (gameinfo.gametype == GAME_Doom)
	{
        screen->DrawTexture (TexMan["M_DOOM"], 94, 2, DTA_Clean, true, TAG_DONE);
	}
	else
	{
        screen->DrawTexture (TexMan["M_STRIFE"], 84, 2, DTA_Clean, true, TAG_DONE);
	}
}

void M_DrawHereticMainMenu ()
{
	char name[9];

	screen->DrawTexture (TexMan["M_HTIC"], 88, 0, DTA_Clean, true, TAG_DONE);

	if (gameinfo.gametype == GAME_Hexen)
	{
		int frame = (MenuTime / 5) % 7;

		sprintf (name, "FBUL%c0", (frame+2)%7 + 'A');
		screen->DrawTexture (TexMan[name], 37, 80, DTA_Clean, true, TAG_DONE);

		sprintf (name, "FBUL%c0", frame + 'A');
		screen->DrawTexture (TexMan[name], 278, 80, DTA_Clean, true, TAG_DONE);
	}
	else
	{
		int frame = (MenuTime / 3) % 18;

		sprintf (name, "M_SKL%.2d", 17 - frame);
		screen->DrawTexture (TexMan[name], 40, 10, DTA_Clean, true, TAG_DONE);

		sprintf (name, "M_SKL%.2d", frame);
		screen->DrawTexture (TexMan[name], 232, 10, DTA_Clean, true, TAG_DONE);
	}
}

//
// M_DrawNewGame
//
void M_DrawNewGame(void)
{
	if (gameinfo.gametype & (GAME_Doom|GAME_Strife))
	{
		screen->DrawTexture (TexMan[gameinfo.gametype == GAME_Doom ? "M_NEWG" : "M_NGAME"], 96, 14, DTA_Clean, true, TAG_DONE);
		screen->DrawTexture (TexMan["M_SKILL"], 54, 38, DTA_Clean, true, TAG_DONE);
	}
}

void M_DrawBotSkill(void)
{
	if ( EpisodeMenu[epi].bBotSkillFullText )
	{
		screen->SetFont( BigFont );
		screen->DrawText( CR_RED, 160 - ( BigFont->StringWidth( EpisodeSkillHeaders[epi] ) / 2 ), 14, EpisodeSkillHeaders[epi], DTA_Clean, true, TAG_DONE );
		screen->SetFont( SmallFont );
	}
	else
		screen->DrawTexture( TexMan[EpisodeSkillHeaders[epi]], 160 - ( TexMan[EpisodeSkillHeaders[epi]]->GetWidth( ) / 2 ), 14, DTA_Clean, true, TAG_DONE );
/*
	if ( epi == 1 )
		screen->DrawTexture( TexMan["M_DMATCH"], 78, 14, DTA_Clean, true, TAG_DONE );
	else
		screen->DrawTexture( TexMan["M_SKTAG"], 78, 14, DTA_Clean, true, TAG_DONE );
*/

	screen->DrawTexture( TexMan["M_BSKILL"], 54, 38, DTA_Clean, true, TAG_DONE );
}

void M_NewGame(int choice)
{
/*
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && !demoplayback)
	{
		M_StartMessage (GStrings("NEWGAME"), NULL, false);
		return;
	}
*/
	// Set up episode menu positioning
	if (gameinfo.gametype & (GAME_Doom|GAME_Strife))
	{
		EpiDef.x = 48;
		EpiDef.y = 63;
	}
	else
	{
		EpiDef.x = 80;
		EpiDef.y = 50;
	}
	if (EpiDef.numitems > 4)
	{
		EpiDef.y -= LINEHEIGHT;
	}
	epi = 0;

	if (gameinfo.gametype == GAME_Hexen && ClassMenuDef.numitems == 0)
	{ // [RH] Make the default entry the last class the player used.
		ClassMenu.lastOn = players[consoleplayer].userinfo.PlayerClass;
		if (ClassMenu.lastOn < 0)
		{
			ClassMenu.lastOn = 3;
		}
		M_SetupNextMenu (&ClassMenu);
	}
	// [GRB] Class select
	else if (ClassMenuDef.numitems > 1)
	{
		ClassMenuDef.lastOn = ClassMenuDef.numitems - 1;
		if (players[consoleplayer].userinfo.PlayerClass >= 0)
		{
			int n = 0;
			for (int i = 0; i < (int)PlayerClasses.Size () && n < 7; i++)
			{
				if (!(PlayerClasses[i].Flags & PCF_NOMENU))
				{
					if (i == players[consoleplayer].userinfo.PlayerClass)
					{
						ClassMenuDef.lastOn = n;
						break;
					}
					n++;
				}
			}
		}

		PickPlayerClass ();

		PlayerState = GetDefaultByType (PlayerClass->Type)->SeeState;
		PlayerTics = PlayerState->GetTics();

		if (FireScreen == NULL)
			FireScreen = new DSimpleCanvas (144, 160);
		M_SetupNextMenu (&ClassMenuDef);
	}
	else if (EpiDef.numitems <= 1)
	{
		if (EpisodeNoSkill[0])
		{
			M_ChooseSkill(2);
		}
		else if (gameinfo.gametype & (GAME_Doom|GAME_Strife))
		{
			if (( EpiDef.numitems == 1 ) && ( EpisodeMenu[0].bBotSkill ))
				M_SetupNextMenu (&BotDef);
			else
				M_SetupNextMenu (&NewDef);
		}
		else
		{
			M_SetupNextMenu (&HereticSkillMenu);
		}
	}
	else
	{
		M_SetupNextMenu (&EpiDef);
	}
}

void M_Multiplayer (int choice)
{
	OptionsActive = M_StartMultiplayerMenu ();
}

//==========================================================================
//
// DrawClassMenu
//
//==========================================================================

static void DrawClassMenu(void)
{
	char name[9];
	int classnum;

	static const char boxLumpName[3][7] =
	{
		"M_FBOX",
		"M_CBOX",
		"M_MBOX"
	};
	static const char walkLumpName[3][10] =
	{
		"M_FWALK%d",
		"M_CWALK%d",
		"M_MWALK%d"
	};

	const char * text = GStrings("MNU_CHOOSECLASS");
	screen->DrawText (CR_UNTRANSLATED, 34, 24, text, DTA_Clean, true, TAG_DONE);
	classnum = itemOn;
	if (classnum > 2)
	{
		classnum = (MenuTime>>2) % 3;
	}
	screen->DrawTexture (TexMan[boxLumpName[classnum]], 174, 8, DTA_Clean, true, TAG_DONE);

	sprintf (name, walkLumpName[classnum], ((MenuTime >> 3) & 3) + 1);
	screen->DrawTexture (TexMan[name], 174+24, 8+12, DTA_Clean, true, TAG_DONE);
}

// [GRB] Class select drawer
static void M_DrawClassMenu ()
{
	int tit_y = 15;
	const char * text = GStrings("MNU_CHOOSECLASS");
	
	if (ClassMenuDef.numitems > 4 && gameinfo.gametype & GAME_Raven)
		tit_y = 2;
	
	screen->DrawText (gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
		160 - BigFont->StringWidth (text)/2,
		tit_y,
		text, DTA_Clean, true, TAG_DONE);

	int x = (200-160)*CleanXfac+(SCREENWIDTH>>1);
	int y = (ClassMenuDef.y-100)*CleanYfac+(SCREENHEIGHT>>1);

	if (!FireScreen)
	{
		screen->Clear (x, y, x + 72 * CleanXfac, y + 80 * CleanYfac-1, 0);
	}
	else
	{
		FireScreen->Lock ();
		M_RenderPlayerBackdrop ();
		M_DrawPlayerBackdrop (x, y - 1);
		FireScreen->Unlock ();
	}

	M_DrawFrame (x, y, 72*CleanXfac, 80*CleanYfac-1);

	spriteframe_t *sprframe = &SpriteFrames[sprites[PlayerState->sprite.index].spriteframes + PlayerState->GetFrame()];
	fixed_t scaleX = GetDefaultByType (PlayerClass->Type)->scaleX;
	fixed_t scaleY = GetDefaultByType (PlayerClass->Type)->scaleY;

	if (sprframe != NULL)
	{
		FTexture *tex = TexMan(sprframe->Texture[0]);
		if (tex != NULL && tex->UseType != FTexture::TEX_Null)
		{
			screen->DrawTexture (tex,
				x + 36*CleanXfac, y + 71*CleanYfac,
				DTA_DestWidth, MulScale16 (tex->GetWidth() * CleanXfac, scaleX),
				DTA_DestHeight, MulScale16 (tex->GetHeight() * CleanYfac, scaleY),
				TAG_DONE);
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawSkillMenu
//
//---------------------------------------------------------------------------

static void DrawHexenSkillMenu()
{
	screen->DrawText (CR_UNTRANSLATED, 74, 16, GStrings("MNU_CHOOSESKILL"), DTA_Clean, true, TAG_DONE);
}


//
//		M_Episode
//
void M_DrawEpisode ()
{
	if (gameinfo.gametype & (GAME_Doom|GAME_Strife))
	{
		screen->DrawTexture (TexMan["M_EPISOD"], 54, 38, DTA_Clean, true, TAG_DONE);
	}
}

void M_VerifyNightmare (int ch)
{
	if (ch != 'y')
		return;
		
	// Tell the server we're leaving the game.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENT_QuitNetworkGame( );

	// Assume normal mode for going through the menu.
	deathmatch = false;
	teamgame = false;

	// Turn campaign mode back on.
	CAMPAIGN_EnableCampaign( );

	gameskill = 4;
	G_DeferedInitNew (EpisodeMaps[epi]);
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;
	M_ClearMenus ();
}

void M_VerifyBotNightmare (int ch)
{
	if (ch != 'y')
		return;

	// Tell the server we're leaving the game.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENT_QuitNetworkGame( );

	// Assume normal mode for going through the menu.
	deathmatch = false;
	teamgame = false;

	// Turn campaign mode back on.
	CAMPAIGN_EnableCampaign( );

	botskill = 4;
	G_DeferedInitNew( EpisodeMaps[epi] );
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;
	M_ClearMenus ();
}

void M_ChooseSkill (int choice)
{
	UCVarValue	Val;

	if (gameinfo.gametype == GAME_Doom && choice == NewDef.numitems - 1)
	{
		M_StartMessage (GStrings("NIGHTMARE"), M_VerifyNightmare, true);
		return;
	}

	gameskill = choice;
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;

	// [BC] Tell the server we're leaving the game.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENT_QuitNetworkGame( );

	// Assume normal mode for going through the menu.
	Val.Bool = false;

	deathmatch.ForceSet( Val, CVAR_Bool );
	teamgame.ForceSet( Val, CVAR_Bool );
	survival.ForceSet( Val, CVAR_Bool );
	invasion.ForceSet( Val, CVAR_Bool );

	// Turn campaign mode back on.
	CAMPAIGN_EnableCampaign( );

	G_DeferedInitNew (EpisodeMaps[epi]);
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;
	M_ClearMenus ();
}

void M_ChooseBotSkill (int choice)
{
	UCVarValue	Val;

	if (gameinfo.gametype & (GAME_Doom|GAME_Strife) && choice == BotDef.numitems - 1)
	{								  
		M_StartMessage( "are you sure? you'll prolly get\nyour ass whooped!\n\npress y or n.", M_VerifyBotNightmare, true );
		return;
	}

	// Tell the server we're leaving the game.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENT_QuitNetworkGame( );

	// Assume normal mode for going through the menu.
	Val.Bool = false;

	deathmatch.ForceSet( Val, CVAR_Bool );
	teamgame.ForceSet( Val, CVAR_Bool );

	// Turn campaign mode back on.
	CAMPAIGN_EnableCampaign( );

	botskill = choice;
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;
	G_DeferedInitNew( EpisodeMaps[epi] );
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;
	M_ClearMenus ();
}

void M_Episode (int choice)
{
	if ((gameinfo.flags & GI_SHAREWARE) && choice)
	{
		if (gameinfo.gametype == GAME_Doom)
		{
			M_StartMessage(GStrings("SWSTRING"),NULL,false);
			//M_SetupNextMenu(&ReadDef);
		}
		else
		{
			showSharewareMessage = 3*TICRATE;
		}
		return;
	}

	epi = choice;

	if (EpisodeNoSkill[choice])
	{
		M_ChooseSkill(2);
		return;
	}

	if (gameinfo.gametype & (GAME_Doom|GAME_Strife))
	{
		if ( EpisodeMenu[epi].bBotSkill )
			M_SetupNextMenu (&BotDef);
		else
			M_SetupNextMenu (&NewDef);
	}
	else if (gameinfo.gametype == GAME_Hexen)
		M_SetupNextMenu (&HexenSkillMenu);
	else
		M_SetupNextMenu (&HereticSkillMenu);
}

//==========================================================================
//
// Sets Hexen's skill menu according to player class
//
//==========================================================================

static void SetHexenSkillMenu (const char * pclass)
{
	if (!stricmp(pclass, "fighter"))
	{
		HexenSkillMenu.x = 120;
		HexenSkillItems[0].name = "MNU_SQUIRE";
		HexenSkillItems[1].name = "MNU_KNIGHT";
		HexenSkillItems[2].name = "MNU_WARRIOR";
		HexenSkillItems[3].name = "MNU_BERSERKER";
		HexenSkillItems[4].name = "MNU_TITAN";
	}
	else if (!stricmp(pclass, "cleric"))
	{
		HexenSkillMenu.x = 116;
		HexenSkillItems[0].name = "MNU_ALTARBOY";
		HexenSkillItems[1].name = "MNU_ACOLYTE";
		HexenSkillItems[2].name = "MNU_PRIEST";
		HexenSkillItems[3].name = "MNU_CARDINAL";
		HexenSkillItems[4].name = "MNU_POPE";
	}
	else if (!stricmp(pclass, "mage"))
	{
		HexenSkillMenu.x = 112;
		HexenSkillItems[0].name = "MNU_APPRENTICE";
		HexenSkillItems[1].name = "MNU_ENCHANTER";
		HexenSkillItems[2].name = "MNU_SORCERER";
		HexenSkillItems[3].name = "MNU_WARLOCK";
		HexenSkillItems[4].name = "MNU_ARCHMAGE";
	}
	else
	{
		// Use Heretic's menu titles as default
		HexenSkillMenu.x = HereticSkillMenu.x;
		HexenSkillItems[0].name = HereticSkillItems[0].name;
		HexenSkillItems[1].name = HereticSkillItems[1].name;
		HexenSkillItems[2].name = HereticSkillItems[2].name;
		HexenSkillItems[3].name = HereticSkillItems[3].name;
		HexenSkillItems[4].name = HereticSkillItems[4].name;
	}
}

//==========================================================================
//
// SCClass
//
//==========================================================================

static void SCClass (int option)
{
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		M_StartMessage (GStrings("NEWGAME"), NULL, false);
		return;
	}

	if (option == 3)
		playerclass = "Random";
	else
		playerclass = PlayerClasses[option].Type->Meta.GetMetaString (APMETA_DisplayName);

	SetHexenSkillMenu(playerclass);

	if (EpiDef.numitems > 1)
	{
		M_SetupNextMenu (&EpiDef);
	}
	else if (!EpisodeNoSkill[0])
	{
		M_SetupNextMenu (&HexenSkillMenu);
	}
	else
	{
		M_ChooseSkill(2);
	}
}

// [GRB]
static void M_ChooseClass (int choice)
{
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		M_StartMessage (GStrings("NEWGAME"), NULL, false);
		return;
	}

	playerclass = (choice < ClassMenuDef.numitems-1) ? ClassMenuItems[choice].name : "Random";
	SetHexenSkillMenu(playerclass);

	if (EpiDef.numitems > 1)
	{
		M_SetupNextMenu (&EpiDef);
	}
	else if (EpisodeNoSkill[0])
	{
		M_ChooseSkill(2);
	}
	else if (gameinfo.gametype & (GAME_Doom|GAME_Strife))
	{
		M_SetupNextMenu (&NewDef);
	}
	else if (gameinfo.gametype == GAME_Hexen)
	{
		M_SetupNextMenu (&HexenSkillMenu);
	}
	else
	{
		M_SetupNextMenu (&HereticSkillMenu);
	}
}


void M_Options (int choice)
{
	OptionsActive = M_StartOptionsMenu ();
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
		S_Sound (CHAN_VOICE, "menu/invalid", 1, ATTN_NONE);
		return;
	}
		
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		M_StartMessage(GStrings("NETEND"),NULL,false);
		return;
	}
		
	M_StartMessage(GStrings("ENDGAME"),M_EndGameResponse,true);
}




//
// M_ReadThis
//
void M_ReadThis (int choice)
{
	drawSkull = false;
	InfoType = 1;
	InfoTic = gametic;
	M_SetupNextMenu (&ReadDef);
}

void M_ReadThisMore (int choice)
{
	InfoType++;
	InfoTic = gametic;
	if (gameinfo.flags & GI_INFOINDEXED)
	{
		if (InfoType >= gameinfo.info.indexed.numPages)
		{
			M_FinishReadThis (0);
		}
	}
	else if (InfoType > 2)
	{
		M_FinishReadThis (0);
	}
}

void M_FinishReadThis (int choice)
{
	drawSkull = true;
	M_PopMenuStack ();
}

//
// M_QuitDOOM
//

void M_QuitResponse(int ch)
{
	if (ch != 'y')
		return;
	if ( NETWORK_GetState( ) == NETSTATE_SINGLE )
	{
		if (gameinfo.quitSounds)
		{
			S_Sound (CHAN_VOICE, gameinfo.quitSounds[(gametic>>2)&7],
				1, ATTN_SURROUND);
			I_WaitVBL (105);
		}
	}
	exit (0);
}

void M_QuitDOOM (int choice)
{
	// We pick index 0 which is language sensitive,
	//  or one at random, between 1 and maximum number.
	if (gameinfo.gametype == GAME_Doom)
	{
		int quitmsg = gametic % NUM_QUITMESSAGES;
		if (quitmsg != 0)
		{
			EndString.Format("QUITMSG%d", quitmsg);
			EndString.Format("%s\n\n%s", GStrings(EndString), GStrings("DOSY"));
		}
		else
		{
			EndString.Format("%s\n\n%s", GStrings("QUITMSG"), GStrings("DOSY"));
		}
	}
	else
	{
		EndString = GStrings("RAVENQUITMSG");
	}

	M_StartMessage (EndString, M_QuitResponse, true);
}

void M_SetupPlayerSetupMenu( void );
extern	ULONG		g_ulPlayerSetupSkin;
extern	ULONG		g_ulPlayerSetupColor;
extern	ULONG		g_ulPlayerSetupClass;

//
// [RH] Player Setup Menu code
//
void M_PlayerSetup (void)
{
	if (( demoplayback ) || ( CLIENTDEMO_IsPlaying( )))
		G_CheckDemoStatus( );

	// Copy all userinfo variables into menu_xxx.
	M_SetupPlayerSetupMenu( );

	M_SwitchMenu( &PlayerSetupMenu );
	PlayerClass = &PlayerClasses[g_ulPlayerSetupClass];
	PlayerSkin = g_ulPlayerSetupSkin;
	R_GetPlayerTranslation (g_ulPlayerSetupColor, &skins[g_ulPlayerSetupSkin], translationtables[TRANSLATION_Players] + 256 * MAXPLAYERS);
	PlayerState = GetDefaultByType( PlayerClass->Type )->SeeState;
	PlayerTics = PlayerState->GetTics( );
	if ( FireScreen == NULL )
		FireScreen = new DSimpleCanvas( 144, 160 );
}

static void M_PlayerSetupTicker (void)
{
	// Based on code in f_finale.c
	FPlayerClass *oldclass = PlayerClass;

	if (currentMenu == &ClassMenuDef)
	{
		int item;

		if (itemOn < ClassMenuDef.numitems-1)
			item = itemOn;
		else
			item = (MenuTime>>2) % (ClassMenuDef.numitems-1);

		PlayerClass = &PlayerClasses[D_PlayerClassToInt (ClassMenuItems[item].name)];
	}
	else
	{
		PickPlayerClass ();
	}

	if (PlayerClass != oldclass)
	{
		PlayerState = GetDefaultByType (PlayerClass->Type)->SeeState;
		PlayerTics = PlayerState->GetTics();

		PlayerSkin = R_FindSkin (skins[PlayerSkin].name, PlayerClass - &PlayerClasses[0]);
		R_GetPlayerTranslation (players[consoleplayer].userinfo.color,
			&skins[PlayerSkin], translationtables[TRANSLATION_Players] + 256 * MAXPLAYERS);
	}

	if (PlayerState->GetTics () != -1 && PlayerState->GetNextState () != NULL)
	{
		if (--PlayerTics > 0)
			return;

		PlayerState = PlayerState->GetNextState();
		PlayerTics = PlayerState->GetTics();
	}
}

static void M_PlayerSetupDrawer( void )
{
	int		xo, yo;
	EColorRange label, value;
	USHORT	usLineHeight;
	USHORT	usOldPlayerSetupXOffset;
	USHORT	usOldPlayerSetupYOffset;
	
	usOldPlayerSetupXOffset = 72;
	usOldPlayerSetupYOffset = 36; // [RC] Move the player display up a bit so the text doesn't overlap

	if ( gameinfo.gametype != GAME_Doom )
		usOldPlayerSetupYOffset -= 7;

	usLineHeight = SmallFont->GetHeight( );

	if ( (gameinfo.gametype & (GAME_Doom|GAME_Strife)) == false )
	{
		xo = 5;
		yo = 5;
		label = CR_GREEN;
		value = CR_UNTRANSLATED;
	}
	else
	{
		xo = yo = 0;
		label = CR_UNTRANSLATED;
		value = CR_GREY;
	}

	// Make sure the font is set when we display "Press space", etc.
	screen->SetFont( SmallFont );

	// Draw player character

	// This part draws the backdrop.
	{
		int x = 320 - 88 - 32 + xo, y = usOldPlayerSetupYOffset + usLineHeight*3 - 18 + yo;

		x = (x-160)*CleanXfac+(SCREENWIDTH>>1);
		y = (y-100)*CleanYfac+(SCREENHEIGHT>>1);
		if (!FireScreen)
		{
			screen->Clear (x, y, x + 72 * CleanXfac, y + 80 * CleanYfac-1, 0);
		}
		else
		{
			FireScreen->Lock ();
			M_RenderPlayerBackdrop ();
			M_DrawPlayerBackdrop (x, y - 1);
			FireScreen->Unlock ();
		}

		M_DrawFrame( x, y, 72 * CleanXfac, 80 * CleanYfac - 1 );
	}

	// This part renders the actual character.
	{
		spriteframe_t *sprframe;
		// [GZDoom]
		fixed_t Scale;
		//int scale;
		
		if (gameinfo.gametype != GAME_Hexen)
		{
			sprframe =
				&SpriteFrames[sprites[skins[g_ulPlayerSetupSkin].sprite].spriteframes + PlayerState->GetFrame()];
			// [GZDoom]
			Scale = skins[g_ulPlayerSetupSkin].Scale;
			//scale = skins[g_ulPlayerSetupSkin].scale + 1;
		}
		else
		{
			sprframe = &SpriteFrames[sprites[PlayerState->sprite.index].spriteframes + PlayerState->GetFrame()];
			// [GZDoom]
			Scale = GetDefault<APlayerPawn>()->scaleX;
			//scale = GetDefault<APlayerPawn>()->xscale + 1;
		}

		if (sprframe != NULL)
		{
			FTexture *tex = TexMan(sprframe->Texture[0]);

			if (tex != NULL && tex->UseType != FTexture::TEX_Null)
			{
				if (tex->Rotations != 0xFFFF)
				{
					tex = TexMan(SpriteFrames[tex->Rotations].Texture[PlayerRotation]);
				}

				// Build the translation for the character that's going to draw.
				// [BB] Changed to use the ZDoom way to display player menu color
				R_GetPlayerTranslation (g_ulPlayerSetupColor,
					&skins[g_ulPlayerSetupSkin], translationtables[TRANSLATION_Players] + 256 * MAXPLAYERS);
				//R_BuildPlayerSetupPlayerTranslation( g_ulPlayerSetupColor, &skins[g_ulPlayerSetupSkin] );

				screen->DrawTexture (tex,
					(320 - 52 - 32 + xo - 160)*CleanXfac + (SCREENWIDTH)/2,
					(usOldPlayerSetupYOffset + usLineHeight*3 + 57 - 104)*CleanYfac + (SCREENHEIGHT/2),
					// [GZDoom]
					DTA_DestWidth, MulScale16 (tex->GetWidth() * CleanXfac, Scale),
					DTA_DestHeight, MulScale16 (tex->GetHeight() * CleanYfac, Scale),
					//DTA_DestWidth, MulScale6 (tex->GetWidth() * CleanXfac, scale),
					//DTA_DestHeight, MulScale6 (tex->GetHeight() * CleanYfac, scale),
					DTA_Translation, translationtables[TRANSLATION_Players] + 256 * MAXPLAYERS,
					//DTA_Translation, translationtables[TRANSLATION_PlayerSetupMenu],
					TAG_DONE);
			}
		}

		const char *str = "PRESS SPACE"; // [RC] Color tweak so it sticks out less
		screen->DrawText( CR_DARKGRAY, 320 - 52 - 32 -
			SmallFont->StringWidth( str ) / 2,
			(USHORT)( usOldPlayerSetupYOffset + usLineHeight * 3 + 69 ), str,
			DTA_Clean, true, TAG_DONE );
		str = PlayerRotation ? "TO SEE FRONT" : "TO SEE BACK";
		screen->DrawText( CR_DARKGRAY, 320 - 52 - 32 -
			SmallFont->StringWidth( str ) / 2,
			(USHORT)( usOldPlayerSetupYOffset + usLineHeight * 4 + 69 ), str,
			DTA_Clean, true, TAG_DONE );
	}
}

// Something cut out from a scan and resized to fit in 32x32. Guess what it is.
static BYTE naru[1024] =
{
	 11,11,11,11,13,15,18,17,16,16,15,14,11, 7, 6,11,14,12,10,10,12,15,13,11, 8, 8,10,13,14,12, 7,16,
	 17,17,14,12,12,15,10,10, 9,10,10, 7, 6, 3, 1, 6, 9, 9, 5, 6,11,13,11, 8, 9,11, 9,11,14,14, 6,12,
	 20,19,17,13, 9, 6, 5, 5, 4, 3, 2, 2, 2, 2, 2, 6,12, 8, 1, 1, 7, 6, 6, 9, 2, 9,13,14,15,13, 8,12,
	 21,20,19,13, 7, 7,11, 8, 4, 2, 8, 6, 2, 0, 1, 7,13,11, 8, 8, 4, 6, 0,14, 7, 6,18,18,15,14,12, 9,
	 19,19,16,12,11,19,14,11, 7, 8,13, 4, 2, 0, 0, 8,14,16,10,14, 5,13, 4,11,14, 6,14,24,19,17,15, 9,
	 18,16,14,14,19,26,14,13,10,22,15, 6, 4, 0, 1, 6,14,19,14,14,11,12,10,10,18,11,12,21,21,16,16,17,
	 18,14,10,10,26,23, 7,10,15,25,16,11,10, 3, 3, 6,12,21,16,17,15, 9,14, 4,19,13,10,12,14,15,16,17,
	 21, 9, 5,15,22,12, 2, 8,17,13,13,15,11, 4, 6, 6,10,19,16, 9,17,11,14, 1,12,14, 7,13, 9,14,16,19,
	 22,10, 6,11,14, 5, 4, 7,10, 8,14,14, 7, 4, 5, 5, 5,15,13, 7,10, 7,10, 5, 6,17, 8,13, 7,10,17,20,
	 21, 7, 6, 8, 6, 6, 6, 5, 6, 7,16,13, 5,10, 8, 5, 3, 7, 9, 6, 4, 3, 3, 5, 4,14, 8,12, 6, 7,17,21,
	 18, 6, 8, 8, 3, 8, 6, 3, 5, 9,17,16, 7,16,11, 4, 5, 4, 8, 7, 2, 3, 0, 4, 4,14, 9, 7, 8, 4,15,22,
	 17,11, 9, 8, 4, 8, 5, 4, 5,13,20,18, 7,16,12,10, 3, 3, 4, 9, 4, 7, 3, 5, 4,14,10, 3, 8, 5,15,22,
	 18,15,11, 8, 3, 9, 3, 3, 5,11,17,17, 7,17,16,14, 4, 5, 6, 9, 7, 1, 2, 6, 6,14, 9, 3, 6, 8,14,22,
	 16,16,11, 6, 4, 8, 1, 1, 9,18,13, 9, 8,18,20,16,11, 6,11, 4, 3, 4, 4, 8, 3,11, 8, 4, 3, 7,11,20,
	 13,14, 9, 4,10, 7, 2, 2,12,17,11, 0, 2,13,24,18,16, 7, 2, 1, 3, 0, 0, 6, 4, 8, 7, 4, 2, 4,10,17,
	 11,11, 7, 5,14, 6, 3, 2, 6, 5, 8, 8, 8, 3,21,26,15, 4, 3,10,16,11, 7, 0, 1, 8, 8, 3, 3, 3,10,15,
	  9,12, 7, 5,13, 3, 3, 1, 1, 5, 8, 9,15,16,10,26,11, 4, 9,13,18,20,18, 0, 0, 6, 9, 4, 2, 3,10,16,
	 10,16, 9, 5,11, 3, 5, 0, 2, 7, 8, 9,13,20,13,25,11,10,16,15,16,17,18,11, 0, 6, 9, 4, 0, 3,12,19,
	 19,21,11, 5,13, 4, 7, 0, 6,10,11,10,12,18,14,18, 8,16,21,18,18,17,17,15, 1, 3, 9, 5, 6, 1,14,21,
	 23,22,14, 6,16, 9, 8, 0, 9,14,14,11,10,15,15,12,11,18,21,20,19,19,17,14, 4, 4, 8, 6,18, 4,15,21,
	 22,20,19,11,17,13, 7, 0, 7,17,16,12, 7,11,12,19,20,14,19,18,18,20,17,11, 1, 5, 8, 7,17, 4,15,17,
	 18,17,19,16,15,11, 5, 1, 7,21,18,13, 6, 9, 9,15,14, 8,12,16,17,19,17, 9, 4, 3, 6, 7,13, 5,14,13,
	 15,17,18,17,12, 8, 5, 4, 8,13,18,14, 8, 6, 8,11, 9, 8, 6, 7, 9, 8, 7,12, 5, 2, 1, 3, 4, 4,10,11,
	 16,18,17,16,12, 9, 8, 2, 4,14,10, 8,10,10,13,13, 7, 7,11,12, 9, 8,16,19, 5, 2, 0, 1, 1, 3, 8,10,
	  6, 7, 5, 6,10, 9,11,10, 6, 7,14,16,17,17,16,14, 8, 8,11,16,19,20,22,18, 4, 2, 0, 2, 1, 2,10,10,
	 12,12,10,11,11,12,13,13,11, 5,16,18,17,18,17,13,10, 7,11,17,22,22,21,14, 2, 3, 1, 3, 2, 6, 7, 9,
	 18,18,19,18,13,13,13,12,12, 5, 9,16,16,14,12, 8, 6, 5,10,13,17,23,20, 5, 0, 3, 2, 4, 4, 5,10, 3,
	 15,18,21,22,17,12,12,10,10, 3, 2, 7,12, 8, 8, 8, 9,10,13,17, 9,15, 6, 2, 4, 6, 9, 4, 4, 0, 5, 6,
	 15,17,21,24,18,13,11, 9, 6, 2, 3, 2, 1, 9,12,11,10,10,13,16, 9, 0,11, 6, 1, 7,10, 6, 7, 5, 5, 6,
	 14,15,19,23,19,14,10, 7, 6, 6, 1, 4, 2, 0, 5,10,10, 9,10, 6, 6, 5,13, 8, 2, 5, 8, 4, 8, 8, 5, 5,
	 15,14,16,21,17,11, 6, 4, 7, 2, 5, 6, 4, 2, 0, 4, 4, 2, 3, 6, 9, 5,10, 8, 1, 5, 5, 3, 5, 4, 2, 4,
	  9, 8,12,16, 9,10, 7, 5, 7, 5, 9, 7, 6, 4, 5, 8, 5, 4, 6, 8, 8, 4, 8, 8, 3, 5, 6, 4, 3, 4, 6, 6,
};

// Just a 32x32 cloud rendered with the standard Photoshop filter
static BYTE smoke[1024] =
{
	  9, 9, 8, 8, 8, 8, 6, 6,13,13,11,21,19,21,23,18,23,24,19,19,24,17,18,12, 9,14, 8,12,12, 5, 8, 6,
	 11,10, 6, 7, 8, 8, 9,13,10,11,17,15,23,22,23,22,20,26,27,26,17,21,20,14,12, 8,11, 8,11, 7, 8, 7,
	  6, 9,13,13,10, 9,13, 7,12,13,16,19,16,20,22,25,22,25,27,22,21,23,15,10,14,14,15,13,12, 8,12, 6,
	  6, 7,12,12,12,16, 9,12,12,15,16,11,21,24,19,24,23,26,28,27,26,21,14,15, 7, 7,10,15,12,11,10, 9,
	  7,14,11,16,12,18,16,14,16,14,11,14,15,21,23,17,20,18,26,24,27,18,20,11,11,14,10,17,17,10, 6,10,
	 13, 9,14,10,13,11,14,15,18,15,15,12,19,19,20,18,22,20,19,22,19,19,19,20,17,15,15,11,16,14,10, 8,
	 13,16,12,16,17,19,17,18,15,19,14,18,15,14,15,17,21,19,23,18,23,22,18,18,17,15,15,16,12,12,15,10,
	 10,12,14,10,16,11,18,15,21,20,20,17,18,19,16,19,14,20,19,14,19,25,22,21,22,24,18,12, 9, 9, 8, 6,
	 10,10,13, 9,15,13,20,19,22,18,18,17,17,21,21,13,13,12,19,18,16,17,27,26,22,23,20,17,12,11, 8, 9,
	  7,13,14,15,11,13,18,22,19,23,23,20,22,24,21,14,12,16,17,19,18,18,22,18,24,23,19,17,16,14, 8, 7,
	 12,12, 8, 8,16,20,26,25,28,28,22,29,23,22,21,18,13,16,15,15,20,17,25,24,19,17,17,17,15,10, 8, 9,
	  7,12,15,11,17,20,25,25,25,29,30,31,28,26,18,16,17,18,20,21,22,20,23,19,18,19,10,16,16,11,11, 8,
	  5, 6, 8,14,14,17,17,21,27,23,27,31,27,22,23,21,19,19,21,19,20,19,17,22,13,17,12,15,10,10,12, 6,
	  8, 9, 8,14,15,16,15,18,27,26,23,25,23,22,18,21,20,17,19,20,20,16,20,14,15,13,12, 8, 8, 7,11,13,
	  7, 6,11,11,11,13,15,22,25,24,26,22,24,26,23,18,24,24,20,18,20,16,17,12,12,12,10, 8,11, 9, 6, 8,
	  9,10, 9, 6, 5,14,16,19,17,21,26,20,23,19,19,17,20,21,26,25,23,21,17,13,12, 5,13,11, 7,12,10,12,
	  6, 5, 4,10,11, 9,10,13,17,20,20,18,23,26,27,20,21,24,20,19,24,20,18,10,11, 3, 6,13, 9, 6, 8, 8,
	  1, 2, 2,11,13,13,11,16,16,16,19,21,20,23,22,28,21,20,19,18,23,16,18, 7, 5, 9, 7, 6, 5,10, 8, 8,
	  0, 0, 6, 9,11,15,12,12,19,18,19,26,22,24,26,30,23,22,22,16,20,19,12,12, 3, 4, 6, 5, 4, 7, 2, 4,
	  2, 0, 0, 7,11, 8,14,13,15,21,26,28,25,24,27,26,23,24,22,22,15,17,12, 8,10, 7, 7, 4, 0, 5, 0, 1,
	  1, 2, 0, 1, 9,14,13,10,19,24,22,29,30,28,30,30,31,23,24,19,17,14,13, 8, 8, 8, 1, 4, 0, 0, 0, 3,
	  5, 2, 4, 2, 9, 8, 8, 8,18,23,20,27,30,27,31,25,28,30,28,24,24,15,11,14,10, 3, 4, 3, 0, 0, 1, 3,
	  9, 3, 4, 3, 5, 6, 8,13,14,23,21,27,28,27,28,27,27,29,30,24,22,23,13,15, 8, 6, 2, 0, 4, 3, 4, 1,
	  6, 5, 5, 3, 9, 3, 6,14,13,16,23,26,28,23,30,31,28,29,26,27,21,20,15,15,13, 9, 1, 0, 2, 0, 5, 8,
	  8, 4, 3, 7, 2, 0,10, 7,10,14,21,21,29,28,25,27,30,28,25,24,27,22,19,13,10, 5, 0, 0, 0, 0, 0, 7,
	  7, 6, 7, 0, 2, 2, 5, 6,15,11,19,24,22,29,27,31,30,30,31,28,23,18,14,14, 7, 5, 0, 0, 1, 0, 1, 0,
	  5, 5, 5, 0, 0, 4, 5,11, 7,10,13,20,21,21,28,31,28,30,26,28,25,21, 9,12, 3, 3, 0, 2, 2, 2, 0, 1,
	  3, 3, 0, 2, 0, 3, 5, 3,11,11,16,19,19,27,26,26,30,27,28,26,23,22,16, 6, 2, 2, 3, 2, 0, 2, 4, 0,
	  0, 0, 0, 3, 3, 1, 0, 4, 5, 9,11,16,24,20,28,26,28,24,28,25,22,21,16, 5, 7, 5, 7, 3, 2, 3, 3, 6,
	  0, 0, 2, 0, 2, 0, 4, 3, 8,12, 9,17,16,23,23,27,27,22,26,22,21,21,13,14, 5, 3, 7, 3, 2, 4, 6, 1,
	  2, 5, 6, 4, 0, 1, 5, 8, 7, 6,15,17,22,20,24,28,23,25,20,21,18,16,13,15,13,10, 8, 5, 5, 9, 3, 7,
	  7, 7, 0, 5, 1, 6, 7, 9,12, 9,12,21,22,25,24,22,23,25,24,18,24,22,17,13,10, 9,10, 9, 6,11, 6, 5,
};

static void M_RenderPlayerBackdrop ()
{
	BYTE *from;
	int width, height, pitch;

	width = FireScreen->GetWidth();
	height = FireScreen->GetHeight();
	pitch = FireScreen->GetPitch();

	int x, y;
	static angle_t time1 = ANGLE_1*180;
	static angle_t time2 = ANGLE_1*56;
	static angle_t time3 = ANGLE_1*99;
	static angle_t time4 = ANGLE_1*1;
	static angle_t t1ang = ANGLE_90;
	static angle_t t2ang = 0;
	static angle_t z1ang = 0;
	static angle_t z2ang = ANGLE_90/2;

	const angle_t a1add = ANGLE_1/2;
	const angle_t a2add = ANGLE_MAX-ANGLE_1;
	const angle_t a3add = ANGLE_1*5/7;
	const angle_t a4add = ANGLE_MAX-ANGLE_1*4/3;

	const angle_t t1add = ANGLE_MAX-ANGLE_1*2;
	const angle_t t2add = ANGLE_MAX-ANGLE_1*3+ANGLE_1/6;
	const angle_t t3add = ANGLE_1*16/7;
	const angle_t t4add = ANGLE_MAX-ANGLE_1*2/3;
	const angle_t x1add = 5<<ANGLETOFINESHIFT;
	const angle_t x2add = ANGLE_MAX-(13<<ANGLETOFINESHIFT);
	const angle_t z1add = 3<<ANGLETOFINESHIFT;
	const angle_t z2add = 4<<ANGLETOFINESHIFT;

	angle_t a1, a2, a3, a4;
	fixed_t c1, c2, c3, c4;
	DWORD tx, ty, tc, ts;
	DWORD ux, uy, uc, us;
	DWORD ltx, lty, lux, luy;

	from = FireScreen->GetBuffer ();

	a3 = time3;
	a4 = time4;

	fixed_t z1 = (finecosine[z2ang>>ANGLETOFINESHIFT]>>2)+FRACUNIT/2;
	fixed_t z2 = (finecosine[z1ang>>ANGLETOFINESHIFT]>>2)+FRACUNIT*3/4;

	tc = MulScale5 (finecosine[t1ang>>ANGLETOFINESHIFT], z1);
	ts = MulScale5 (finesine[t1ang>>ANGLETOFINESHIFT], z1);
	uc = MulScale5 (finecosine[t2ang>>ANGLETOFINESHIFT], z2);
	us = MulScale5 (finesine[t2ang>>ANGLETOFINESHIFT], z2);

	ltx = -width/2*tc;
	lty = -width/2*ts;
	lux = -width/2*uc;
	luy = -width/2*us;

	for (y = 0; y < height; ++y)
	{
		a1 = time1;
		a2 = time2;
		c3 = finecosine[a3>>ANGLETOFINESHIFT];
		c4 = finecosine[a4>>ANGLETOFINESHIFT];
		tx = ltx - (y-height/2)*ts;
		ty = lty + (y-height/2)*tc;
		ux = lux - (y-height/2)*us;
		uy = luy + (y-height/2)*uc;
		for (x = 0; x < width; ++x)
		{
			c1 = finecosine[a1>>ANGLETOFINESHIFT];
			c2 = finecosine[a2>>ANGLETOFINESHIFT];
			from[x] = ((c1 + c2 + c3 + c4) >> (FRACBITS+3-7)) + 128	// plasma
			 + naru[(tx>>27)+((ty>>22)&992)]						// rotozoomer 1
			 + smoke[(ux>>27)+((uy>>22)&992)];						// rotozoomer 2
			tx += tc;
			ty += ts;
			ux += uc;
			uy += us;
			a1 += a1add;
			a2 += a2add;
		}
		a3 += a3add;
		a4 += a4add;
		from += pitch;
	}

	time1 += t1add;
	time2 += t2add;
	time3 += t3add;
	time4 += t4add;
	t1ang += x1add;
	t2ang += x2add;
	z1ang += z1add;
	z2ang += z2add;
}

static void M_DrawPlayerBackdrop (int x, int y)
{
	DCanvas *src = FireScreen;
	DCanvas *dest = screen;
	BYTE *destline, *srcline;
	const int destwidth = src->GetWidth() * CleanXfac / 2;
	const int destheight = src->GetHeight() * CleanYfac / 2;
	const int desty = y;
	const int destx = x;
	const fixed_t fracxstep = FRACUNIT*2 / CleanXfac;
	const fixed_t fracystep = FRACUNIT*2 / CleanYfac;
	fixed_t fracx, fracy = 0;


	if (currentrenderer == 1)
	{
		// Why? :(
		int srcW, srcH;
		srcW = src->GetWidth();
		srcH = src->GetHeight();
		BYTE *img = new BYTE[srcW * srcH];
		BYTE *srcImg = src->GetBuffer();
		for (y = 0; y < srcH; y++)
		{
			for (x = 0; x < srcW; x++)
			{
				img[x + (y * srcW)] = FireRemap[srcImg[x + (y * srcW)]];
			}
		}
		gl_DrawBuffer(img, srcW, srcH, destx, desty, destwidth, destheight, NULL);
		delete [] img;
		return;
	}

	if (fracxstep == FRACUNIT)
	{
		for (y = desty; y < desty + destheight; y++, fracy += fracystep)
		{
			srcline = src->GetBuffer() + (fracy >> FRACBITS) * src->GetPitch();
			destline = dest->GetBuffer() + y * dest->GetPitch() + destx;

			for (x = 0; x < destwidth; x++)
			{
				destline[x] = FireRemap[srcline[x]];
			}
		}
	}
	else
	{
		for (y = desty; y < desty + destheight; y++, fracy += fracystep)
		{
			srcline = src->GetBuffer() + (fracy >> FRACBITS) * src->GetPitch();
			destline = dest->GetBuffer() + y * dest->GetPitch() + destx;
			for (x = fracx = 0; x < destwidth; x++, fracx += fracxstep)
			{
				destline[x] = FireRemap[srcline[fracx >> FRACBITS]];
			}
		}
	}
}

void SendNewColor (int red, int green, int blue)
{
	char command[24];

	sprintf( command, "%02x %02x %02x", red, green, blue );
	g_ulPlayerSetupColor = V_GetColorFromString( NULL, command );

	sprintf (command, "menu_color \"%02x %02x %02x\"", red, green, blue );
	C_DoCommand (command);

	R_GetPlayerTranslation (MAKERGB (red, green, blue), &skins[g_ulPlayerSetupSkin], translationtables[TRANSLATION_Players] + 256 * MAXPLAYERS);
}

//
//		Menu Functions
//
void M_StartMessage (const char *string, void (*routine)(int), bool input)
{
	C_HideConsole ();
	messageLastMenuActive = menuactive;
	messageToPrint = 1;
	messageString = string;
	messageRoutine = routine;
	messageNeedsInput = input;
	if (menuactive == MENU_Off)
	{
		M_ActivateMenuInput ();
	}
	if (input)
	{
		S_StopSound ((AActor *)NULL, CHAN_VOICE);
		S_Sound (CHAN_VOICE, "menu/prompt", 1, ATTN_NONE);
	}
	return;
}



//
//		Find string height from hu_font chars
//
int M_StringHeight (const char *string)
{
	int h;
	int height = screen->Font->GetHeight ();
		
	h = height;
	while (*string)
	{
		if ((*string++) == '\n')
			h += height;
	}
				
	return h;
}



//
// CONTROL PANEL
//

// [BC] God this is gross.
extern	menu_t	PlayerSetupMenu;

//
// M_Responder
//
bool M_Responder (event_t *ev)
{
	int ch;
	int i;

	ch = -1;

	if (menuactive == MENU_Off && ev->type == EV_KeyDown)
	{
		// Pop-up menu?
		if (ev->data1 == KEY_ESCAPE)
		{
			M_StartControlPanel (true);
			M_SetupNextMenu (TopLevelMenu);
			return true;
		}
		// If devparm is set, pressing F1 always takes a screenshot no matter
		// what it's bound to. (for those who don't bother to read the docs)
		if (devparm && ev->data1 == KEY_F1)
		{
			G_ScreenShot (NULL);
			return true;
		}
	}
	else if (ev->type == EV_GUI_Event && menuactive == MENU_On && CHAT_GetChatMode( ) == 0 )
	{
		if (ev->subtype == EV_GUI_KeyDown || ev->subtype == EV_GUI_KeyRepeat)
		{
			ch = ev->data1;
		}
		else if (ev->subtype == EV_GUI_Char && genStringEnter)
		{
			ch = ev->data1;
			if (saveCharIndex < genStringLen &&
				(genStringEnter==2 || (size_t)SmallFont->StringWidth (savegamestring) < (genStringLen-1)*8))
			{
				savegamestring[saveCharIndex] = ch;
				savegamestring[++saveCharIndex] = 0;
			}
			return true;
		}
	}
	
	if (OptionsActive && CHAT_GetChatMode( ) == 0 )
	{
		M_OptResponder (ev);
		return true;
	}
	
	if (ch == -1)
		return false;

	// Save Game string input
	// [RH] and Player Name string input
	if (genStringEnter)
	{
		switch (ch)
		{
		case '\b':
			if (saveCharIndex > 0)
			{
				saveCharIndex--;
				savegamestring[saveCharIndex] = 0;
			}
			break;

		case GK_ESCAPE:
			genStringEnter = 0;
			genStringCancel ();	// [RH] Function to call when escape is pressed
			break;
								
		case '\r':
			if (savegamestring[0])
			{
				genStringEnter = 0;
				if (messageToPrint)
					M_ClearMenus ();
				genStringEnd (SelSaveGame);	// [RH] Function to call when enter is pressed
			}
			break;
		}
		return true;
	}

	// Take care of any messages that need input
	if (messageToPrint)
	{
		ch = tolower (ch);
		if (messageNeedsInput &&
			ch != ' ' && ch != 'n' && ch != 'y' && ch != GK_ESCAPE)
		{
			return false;
		}

		menuactive = messageLastMenuActive;
		messageToPrint = 0;
		if (messageRoutine)
			messageRoutine (ch);

		if (menuactive != MENU_Off)
		{
			M_DeactivateMenuInput ();
		}
		SB_state = screen->GetPageCount ();	// refresh the statbar
		BorderNeedRefresh = screen->GetPageCount ();
		S_Sound (CHAN_VOICE, "menu/dismiss", 1, ATTN_NONE);
		return true;
	}

	if (ch == GK_ESCAPE)
	{
		// [RH] Escape now moves back one menu instead of quitting
		//		the menu system. Thus, backspace is ignored.
		currentMenu->lastOn = itemOn;
		M_PopMenuStack ();
		return true;
	}

	if (currentMenu == &SaveDef || currentMenu == &LoadDef)
	{
		return M_SaveLoadResponder (ev);
	}

	// Keys usable within menu
	switch (ch)
	{
	case GK_DOWN:
		do
		{
			if (itemOn+1 > currentMenu->numitems-1)
				itemOn = 0;
			else itemOn++;
			S_Sound (CHAN_VOICE, "menu/cursor", 1, ATTN_NONE);
		} while(currentMenu->menuitems[itemOn].status==-1);
		return true;

	case GK_UP:
		do
		{
			if (!itemOn)
				itemOn = currentMenu->numitems-1;
			else itemOn--;
			S_Sound (CHAN_VOICE, "menu/cursor", 1, ATTN_NONE);
		} while(currentMenu->menuitems[itemOn].status==-1);
		return true;

	case GK_LEFT:
		if (currentMenu->menuitems[itemOn].routine &&
			currentMenu->menuitems[itemOn].status == 2)
		{
			S_Sound (CHAN_VOICE, "menu/change", 1, ATTN_NONE);
			currentMenu->menuitems[itemOn].routine(0);
		}
		return true;

	case GK_RIGHT:
		if (currentMenu->menuitems[itemOn].routine &&
			currentMenu->menuitems[itemOn].status == 2)
		{
			S_Sound (CHAN_VOICE, "menu/change", 1, ATTN_NONE);
			currentMenu->menuitems[itemOn].routine(1);
		}
		return true;

	case '\r':
		if (currentMenu->menuitems[itemOn].routine &&
			currentMenu->menuitems[itemOn].status)
		{
			currentMenu->lastOn = itemOn;
			if (currentMenu->menuitems[itemOn].status == 2)
			{
				currentMenu->menuitems[itemOn].routine(1);		// right arrow
				S_Sound (CHAN_VOICE, "menu/change", 1, ATTN_NONE);
			}
			else
			{
				currentMenu->menuitems[itemOn].routine(itemOn);
				S_Sound (CHAN_VOICE, "menu/choose", 1, ATTN_NONE);
			}
		}
		return true;

	case ' ':
		//if (currentMenu == &PSetupDef)
		if ( 0 )
		{
			PlayerRotation ^= 8;
			break;
		}
		// intentional fall-through

	default:
		if (ch)
		{
			ch = tolower (ch);
			for (i = (itemOn + 1) % currentMenu->numitems;
				 i != itemOn;
				 i = (i + 1) % currentMenu->numitems)
			{
				if (currentMenu->menuitems[i].alphaKey == ch)
				{
					break;
				}
			}
			if (currentMenu->menuitems[i].alphaKey == ch)
			{
				itemOn = i;
				S_Sound (CHAN_VOICE, "menu/cursor", 1, ATTN_NONE);
				return true;
			}
		}
		break;
	}

	// [RH] Menu eats all keydown events while active
	return (ev->subtype == EV_GUI_KeyDown);
}

bool M_SaveLoadResponder (event_t *ev)
{
	char workbuf[512];

	if (SelSaveGame != NULL && SelSaveGame->Succ != NULL)
	{
		switch (ev->data1)
		{
		case GK_F1:
			if (!SelSaveGame->Filename.IsEmpty())
			{
				sprintf (workbuf, "File on disk:\n%s", SelSaveGame->Filename.GetChars());
				if (SaveComment != NULL)
				{
					V_FreeBrokenLines (SaveComment);
				}
				SaveComment = V_BreakLines (screen->Font, 216*screen->GetWidth()/640/CleanXfac, workbuf);
			}
			break;

		case GK_UP:
			if (SelSaveGame != SaveGames.Head)
			{
				if (SelSaveGame == TopSaveGame)
				{
					TopSaveGame = static_cast<FSaveGameNode *>(TopSaveGame->Pred);
				}
				SelSaveGame = static_cast<FSaveGameNode *>(SelSaveGame->Pred);
			}
			else
			{
				SelSaveGame = static_cast<FSaveGameNode *>(SaveGames.TailPred);
			}
			M_UnloadSaveData ();
			M_ExtractSaveData (SelSaveGame);
			break;

		case GK_DOWN:
			if (SelSaveGame != SaveGames.TailPred)
			{
				SelSaveGame = static_cast<FSaveGameNode *>(SelSaveGame->Succ);
			}
			else
			{
				SelSaveGame = TopSaveGame =
					static_cast<FSaveGameNode *>(SaveGames.Head);
			}
			M_UnloadSaveData ();
			M_ExtractSaveData (SelSaveGame);
			break;

		case GK_DEL:
		case '\b':
			if (SelSaveGame != &NewSaveNode)
			{
				EndString.Format("%s" TEXTCOLOR_WHITE "%s" TEXTCOLOR_NORMAL "?\n\n%s",
					GStrings("MNU_DELETESG"), SelSaveGame->Title, GStrings("PRESSYN"));
					
				M_StartMessage (EndString, M_DeleteSaveResponse, true);
			}
			break;

		case 'N':
			if (currentMenu == &SaveDef)
			{
				SelSaveGame = TopSaveGame = &NewSaveNode;
				M_UnloadSaveData ();
			}
			break;

		case '\r':
			if (currentMenu == &LoadDef)
			{
				M_LoadSelect (SelSaveGame);
			}
			else
			{
				M_SaveSelect (SelSaveGame);
			}
		}
	}

	return (ev->subtype == EV_GUI_KeyDown);
}

static void M_LoadSelect (const FSaveGameNode *file)
{
	G_LoadGame (file->Filename.GetChars());
	if (gamestate == GS_FULLCONSOLE)
	{
		gamestate = GS_HIDECONSOLE;
	}
	if (quickSaveSlot == (FSaveGameNode *)1)
	{
		quickSaveSlot = SelSaveGame;
	}
	M_ClearMenus ();
	BorderNeedRefresh = screen->GetPageCount ();
}


//
// User wants to save. Start string input for M_Responder
//
static void M_SaveSelect (const FSaveGameNode *file)
{
	// we are going to be intercepting all chars
	genStringEnter = 1;
	genStringEnd = M_DoSave;
	genStringCancel = M_ClearMenus;
	genStringLen = SAVESTRINGSIZE-1;

	if (file != &NewSaveNode)
	{
		strcpy (savegamestring, file->Title);
	}
	else
	{
		savegamestring[0] = 0;
	}
	saveCharIndex = strlen (savegamestring);
}

static void M_DeleteSaveResponse (int choice)
{
	M_ClearSaveStuff ();
	if (choice == 'y')
	{
		FSaveGameNode *next = static_cast<FSaveGameNode *>(SelSaveGame->Succ);
		if (next->Succ == NULL)
		{
			next = static_cast<FSaveGameNode *>(SelSaveGame->Pred);
			if (next->Pred == NULL)
			{
				next = NULL;
			}
		}

		remove (SelSaveGame->Filename.GetChars());
		M_UnloadSaveData ();
		SelSaveGame = M_RemoveSaveSlot (SelSaveGame);
		M_ExtractSaveData (SelSaveGame);
	}
}

//
// M_StartControlPanel
//
void M_StartControlPanel (bool makeSound)
{
	// intro might call this repeatedly
	if (menuactive == MENU_On)
		return;
	
	drawSkull = true;
	MenuStackDepth = 0;
	currentMenu = TopLevelMenu;
	itemOn = currentMenu->lastOn;
	C_HideConsole ();				// [RH] Make sure console goes bye bye.
	OptionsActive = false;			// [RH] Make sure none of the options menus appear.
	M_ActivateMenuInput ();

	if (makeSound)
	{
		S_Sound (CHAN_VOICE, "menu/activate", 1, ATTN_NONE);
	}
}


//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer ()
{
	int i, x, y, max;
	PalEntry fade = 0;

	const player_t *player = &players[consoleplayer];
	if (player->camera != NULL && (gamestate == GS_LEVEL || gamestate == GS_TITLELEVEL))
	{
		if (player->camera->player != NULL)
		{
			player = player->camera->player;
		}
		fade = PalEntry (BYTE(player->BlendA*255), BYTE(player->BlendR*255), BYTE(player->BlendG*255), BYTE(player->BlendB*255));
	}

	// Horiz. & Vertically center string and print it.
	if (messageToPrint)
	{
		screen->Dim (fade);
		BorderNeedRefresh = screen->GetPageCount ();
		SB_state = screen->GetPageCount ();

		FBrokenLines *lines = V_BreakLines (screen->Font, 320, messageString);
		y = 100;

		for (i = 0; lines[i].Width >= 0; i++)
			y -= screen->Font->GetHeight () / 2;

		for (i = 0; lines[i].Width >= 0; i++)
		{
			screen->DrawText (CR_UNTRANSLATED, 160 - lines[i].Width/2, y, lines[i].Text,
				DTA_Clean, true, TAG_DONE);
			y += screen->Font->GetHeight ();
		}

		V_FreeBrokenLines (lines);
	}
	else if (menuactive != MENU_Off)
	{
		if (InfoType == 0 && !OptionsActive)
		{
			screen->Dim (fade);
		}
		// For Heretic shareware message:
		if (showSharewareMessage)
		{
			const char * text = GStrings("MNU_ONLYREGISTERED");
			screen->DrawText (CR_WHITE, 160 - SmallFont->StringWidth(text)/2,
				8, text, DTA_Clean, true, TAG_DONE);
		}

		BorderNeedRefresh = screen->GetPageCount ();
		SB_state = screen->GetPageCount ();

		if (OptionsActive)
		{
			M_OptDrawer ();
		}
		else
		{
			screen->SetFont (BigFont);
			if (currentMenu->routine)
				currentMenu->routine(); 		// call Draw routine

			// DRAW MENU
			x = currentMenu->x;
			y = currentMenu->y;
			max = currentMenu->numitems;

			for (i = 0; i < max; i++)
			{
				if (currentMenu->menuitems[i].name)
				{
					if (currentMenu->menuitems[i].fulltext)
					{
						int color = CR_UNTRANSLATED;
						if ((currentMenu == &EpiDef || currentMenu == &ClassMenuDef) &&
							gameinfo.gametype == GAME_Doom)
						{
							color = CR_RED;
						}
						screen->DrawText (color, x, y,
							GStrings(currentMenu->menuitems[i].name),
							DTA_Clean, true, TAG_DONE);
					}
					else
					{
						screen->DrawTexture (TexMan[currentMenu->menuitems[i].name], x, y,
							DTA_Clean, true, TAG_DONE);
					}
				}
				y += LINEHEIGHT;
			}
			screen->SetFont (SmallFont);
			
			// DRAW CURSOR
			if (drawSkull)
			{
/*
				if (currentMenu == &PSetupDef)
				{
					// [RH] Use options menu cursor for the player setup menu.
					if (skullAnimCounter < 6)
					{
						screen->SetFont (ConFont);
						screen->DrawText (CR_RED, x - 16,
							currentMenu->y + itemOn*LINEHEIGHT +
							(!(gameinfo.gametype & (GAME_Doom|GAME_Strife)) ? 6 : -1), "\xd",
							DTA_Clean, true, TAG_DONE);
						screen->SetFont (SmallFont);
					}
				}
				else*/ if (gameinfo.gametype == GAME_Doom)
				{
					screen->DrawTexture (TexMan[skullName[whichSkull]],
						x + SKULLXOFF, currentMenu->y - 5 + itemOn*LINEHEIGHT,
						DTA_Clean, true, TAG_DONE);
				}
				else if (gameinfo.gametype == GAME_Strife)
				{
					screen->DrawTexture (TexMan[cursName[(MenuTime >> 2) & 7]],
						x - 28, currentMenu->y - 5 + itemOn*LINEHEIGHT,
						DTA_Clean, true, TAG_DONE);
				}
				else
				{
					screen->DrawTexture (TexMan[MenuTime & 16 ? "M_SLCTR1" : "M_SLCTR2"],
						x + SELECTOR_XOFFSET,
						currentMenu->y + itemOn*LINEHEIGHT + SELECTOR_YOFFSET,
						DTA_Clean, true, TAG_DONE);
				}
			}
		}
	}
}


static void M_ClearSaveStuff ()
{
	M_UnloadSaveData ();
	if (SaveGames.Head == &NewSaveNode)
	{
		SaveGames.RemHead ();
		if (SelSaveGame == &NewSaveNode)
		{
			SelSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
		}
		if (TopSaveGame == &NewSaveNode)
		{
			TopSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
		}
	}
	if (quickSaveSlot == (FSaveGameNode *)1)
	{
		quickSaveSlot = NULL;
	}
}

//
// M_ClearMenus
//
void M_ClearMenus ()
{
	if (FireScreen)
	{
		delete FireScreen;
		FireScreen = NULL;
	}
	M_ClearSaveStuff ();
	M_DeactivateMenuInput ();
	MenuStackDepth = 0;
	OptionsActive = false;
	InfoType = 0;
	drawSkull = true;
	M_DemoNoPlay = false;
	BorderNeedRefresh = screen->GetPageCount ();
}




//
// M_SetupNextMenu
//
void M_SetupNextMenu (oldmenu_t *menudef)
{
	MenuStack[MenuStackDepth].menu.old = menudef;
	MenuStack[MenuStackDepth].isNewStyle = false;
	MenuStack[MenuStackDepth].drawSkull = drawSkull;
	MenuStackDepth++;

	currentMenu = menudef;
	itemOn = currentMenu->lastOn;
}


void M_PopMenuStack (void)
{
	M_DemoNoPlay = false;
	InfoType = 0;
	M_ClearSaveStuff ();
	flagsvar = 0;
	if (MenuStackDepth > 1)
	{
		MenuStackDepth -= 2;
		if (MenuStack[MenuStackDepth].isNewStyle)
		{
			OptionsActive = true;
			CurrentMenu = MenuStack[MenuStackDepth].menu.newmenu;
			CurrentItem = CurrentMenu->lastOn;
		}
		else
		{
			OptionsActive = false;
			currentMenu = MenuStack[MenuStackDepth].menu.old;
			itemOn = currentMenu->lastOn;
		}
		drawSkull = MenuStack[MenuStackDepth].drawSkull;
		++MenuStackDepth;
		S_Sound (CHAN_VOICE, "menu/backup", 1, ATTN_NONE);
	}
	else
	{
		M_ClearMenus ();
		S_Sound (CHAN_VOICE, "menu/clear", 1, ATTN_NONE);
	}
}


//
// M_Ticker
//
void M_Ticker (void)
{
	if (showSharewareMessage)
	{
		--showSharewareMessage;
	}
	if (menuactive == MENU_Off)
	{
		return;
	}
	MenuTime++;
	if (--skullAnimCounter <= 0)
	{
		whichSkull ^= 1;
		skullAnimCounter = 8;
	}
	//if (currentMenu == &PSetupDef || currentMenu == &ClassMenuDef)
	if ( CurrentMenu == &PlayerSetupMenu )
		M_PlayerSetupTicker ();
}


//
// M_Init
//
EXTERN_CVAR (Int, screenblocks)

void M_Init (void)
{
	unsigned int i;

	atterm (M_Deinit);

	if (gameinfo.gametype & (GAME_Doom|GAME_Strife))
	{
		TopLevelMenu = currentMenu = &MainDef;
		if (gameinfo.gametype == GAME_Strife)
		{
			MainDef.y = 45;
			NewDef.lastOn = 1;
		}
	}
	else
	{
		TopLevelMenu = currentMenu = &HereticMainDef;
//		PSetupDef.y -= 7;
		LoadDef.y -= 20;
		SaveDef.y -= 20;
	}
	PickPlayerClass ();
	OptionsActive = false;
	menuactive = MENU_Off;
	InfoType = 0;
	itemOn = currentMenu->lastOn;
	whichSkull = 0;
	skullAnimCounter = 10;
	drawSkull = true;
	messageToPrint = 0;
	messageString = NULL;
	messageLastMenuActive = menuactive;
	quickSaveSlot = NULL;
	lastSaveSlot = NULL;
	strcpy (NewSaveNode.Title, "<New Save Game>");

	underscore[0] = (gameinfo.gametype & (GAME_Doom|GAME_Strife)) ? '_' : '[';
	underscore[1] = '\0';

	if (gameinfo.gametype == GAME_Doom)
	{
		LINEHEIGHT = 16;
	}
	else if (gameinfo.gametype == GAME_Strife)
	{
		LINEHEIGHT = 19;
	}
	else
	{
		LINEHEIGHT = 20;
	}

	switch (gameinfo.flags & GI_MENUHACK)
	{
	case GI_MENUHACK_COMMERCIAL:
		MainMenu[MainDef.numitems-2] = MainMenu[MainDef.numitems-1];
		MainDef.numitems--;
		MainDef.y += 8;
		ReadDef.routine = M_DrawReadThis;
		ReadDef.x = 330;
		ReadDef.y = 165;
		ReadMenu[0].routine = M_FinishReadThis;
		break;
	default:
		break;
	}
	M_OptInit ();

	// [GRB] Set up player class menu
	if (!(gameinfo.gametype == GAME_Hexen && PlayerClasses.Size () == 3 &&
		PlayerClasses[0].Type->IsDescendantOf (PClass::FindClass (NAME_FighterPlayer)) &&
		PlayerClasses[1].Type->IsDescendantOf (PClass::FindClass (NAME_ClericPlayer)) &&
		PlayerClasses[2].Type->IsDescendantOf (PClass::FindClass (NAME_MagePlayer))))
	{
		int n = 0;

		for (i = 0; i < PlayerClasses.Size () && n < 7; i++)
		{
			if (!(PlayerClasses[i].Flags & PCF_NOMENU))
			{
				ClassMenuItems[n].name =
					PlayerClasses[i].Type->Meta.GetMetaString (APMETA_DisplayName);
				n++;
			}
		}

		if (n > 1)
		{
			ClassMenuItems[n].name = "Random";
			ClassMenuDef.numitems = n+1;
		}
		else
		{
			if (n == 0)
			{
				ClassMenuItems[0].name =
					PlayerClasses[0].Type->Meta.GetMetaString (APMETA_DisplayName);
			}
			ClassMenuDef.numitems = 1;
		}

		if (gameinfo.gametype & (GAME_Doom|GAME_Strife))
		{
			ClassMenuDef.x = 48;
			ClassMenuDef.y = 63;
		}
		else
		{
			ClassMenuDef.x = 80;
			ClassMenuDef.y = 50;
		}
		if (ClassMenuDef.numitems > 4)
		{
			ClassMenuDef.y -= LINEHEIGHT;
		}
	}

	// [RH] Build a palette translation table for the player setup effect
	if (gameinfo.gametype != GAME_Hexen)
	{
		for (i = 0; i < 256; i++)
		{
			FireRemap[i] = ColorMatcher.Pick (i/2+32, 0, i/4);
		}
	}
	else
	{ // The reddish color ramp above doesn't look too good with the
	  // Hexen palette, so Hexen gets a greenish one instead.
		for (i = 0; i < 256; ++i)
		{
			FireRemap[i] = ColorMatcher.Pick (i/4, i*13/40+7, i/4);
		}
	}

	// Change the "skin" label to "class" in the player menu in Hexen.
	// [BB] 2 is the "skin" label, 6 is the "railgun color" label
	if ( gameinfo.gametype == GAME_Hexen )
		PlayerSetupItems[2].label = "class";
}

static void PickPlayerClass ()
{
	int pclass = 0;

	// [GRB] Pick a class from player class list
	if (PlayerClasses.Size () > 1)
	{
		pclass = players[consoleplayer].userinfo.PlayerClass;

		if (pclass < 0)
		{
			pclass = (MenuTime>>7) % PlayerClasses.Size ();
		}
	}

	PlayerClass = &PlayerClasses[pclass];
}
