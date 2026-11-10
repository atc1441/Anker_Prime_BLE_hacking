//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// Modified 2026 for the DMC-100 port.
//
// DESCRIPTION:
//	DOOM selection menu, options, episode etc.
//	Sliders and icons. Kinda widget stuff.
//
// The MENU overlay (port/game/l_menu.c). The resident glue keeps the
// vanilla names (port/game/fe_glue.c). The overlay has no data of its
// own: the menus are constant, the state of the menu is in fe
// (port/game/fe.h) and the menu globals of the glue. Single player with
// the one episode of the pack: no episode, read-this, detail, screen
// size or volume pages, the options are those of the meter. Load and
// save use the slots of SAVE.DAT, a save is described by its map and
// level time, not by typed text.
//
// A page is drawn on a black panel over the picture of the frame before
// the menu opened, which the LCD keeps (R_ScreenDraw,
// port/render/r_screen.h). The panel is the union of the boxes of the
// pages shown since a frame last drew the picture under the menu.
//


#include <stddef.h>
#include <string.h>

#include "doomdef.h"
#include "doomkeys.h"
#include "dstrings.h"

#include "d_main.h"

#include "i_system.h"
#include "i_timer.h"
#include "v_video.h"
#include "dmcdat.h"
#include "extents.h"
#include "fetch.h"

#include "hu_stuff.h"

#include "g_game.h"

#include "p_mapdata.h"
#include "s_sound.h"

#include "doomstat.h"

// Data.
#include "sounds.h"

#include "m_menu.h"

#include "dmc_fw.h"
#include "fe.h"
#include "platform.h"
#include "r_screen.h"
#include "r_strip.h"


extern boolean		message_dontfuckwithme;

#define SKULLXOFF		-32
#define LINEHEIGHT		16

// The keys of the menu, those the meter sends (port/game/i_input.c):
// the defaults of m_controls.c as constants, so the resident image does
// not keep them for the overlay
#define key_menu_activate	KEY_ESCAPE
#define key_menu_up		KEY_UPARROW
#define key_menu_down		KEY_DOWNARROW
#define key_menu_left		KEY_LEFTARROW
#define key_menu_right		KEY_RIGHTARROW
#define key_menu_back		KEY_BACKSPACE
#define key_menu_forward	KEY_ENTER
#define key_menu_confirm	'y'
#define key_menu_abort		'n'
#define key_menu_quit		KEY_F10

//
// MENU TYPEDEFS
//
typedef struct
{
    // 0 = no cursor here, 1 = ok, 2 = arrows ok
    short	status;

    // picture (dmc_ui id), -1 none
    short	pic;

    // choice = menu item #.
    // if status = 2,
    //   choice=0:leftarrow,1:rightarrow
    void	(*routine)(int choice);
} menuitem_t;



// The menus are constant, the last item of each is fe.last[menu]
typedef struct menu_s
{
    short		numitems;	// # of menu items
    short		prevMenu;	// previous menu, -1 none
    const menuitem_t*	menuitems;	// menu items
    void		(*routine)(void);	// draw routine
    short		x;
    short		y;		// x,y of menu
} menu_t;

enum { MENU_MAIN, MENU_NEW, MENU_OPTIONS, MENU_LOAD, MENU_SAVE };

#define currentMenu		(&menus[fe.menu])
#define itemOn			(fe.item)	// menu item skull is on
#define whichSkull		(fe.skull)	// which skull to draw
#define messageLastMenuActive	(fe.msglast)

// graphic name of skulls
static const short skullName[2] = {DMC_UI_M_SKULL1,DMC_UI_M_SKULL2};

// Messages: a string of the image (fe.msg is its id), or one of this
// overlay. The quit message is QUITMSG + fe.msgarg, an empty line and
// DOSY.
enum
{
    MSG_OWN = 0x80,
    MSG_NOSAVE = MSG_OWN,
    MSG_NONIGHTMARE,
    MSG_USB,
    MSG_BENCH,
    MSG_BADSAVE
};

_Static_assert(DMC_NSTRINGS <= MSG_OWN, "a string id is a message id");

static const char *const ownmessages[] =
{
    "save.dat is missing or not usable",
    "nightmare is not available\non the meter.\n\npress a key.",
    "open the data drive on usb?\nthe game ends. (y/n)",
    "run the benchmark?\nthe game ends. (y/n)",
    "this save is damaged"
};

//
// PROTOTYPES
//
static void M_NewGame(int choice);
static void M_ChooseSkill(int choice);
static void M_Options(int choice);
static void M_EndGame(int choice);
static void M_QuitDOOM(int choice);
static void M_LoadGame(int choice);
static void M_SaveGame(int choice);
static void M_LoadSelect(int choice);
static void M_SaveSelect(int choice);

static void M_ChangeMessages(int choice);
static void M_ChangeSound(int choice);
static void M_UsbDrive(int choice);
static void M_Benchmark(int choice);

static void M_DrawMainMenu(void);
static void M_DrawNewGame(void);
static void M_DrawOptions(void);
static void M_DrawLoad(void);
static void M_DrawSave(void);

static void M_SetupNextMenu(int menu);
static void M_StartMessage(int msg, boolean input);
static void M_ClearMenus (void);




//
// DOOM MENU
//
enum
{
    newgame = 0,
    options,
    loadgame,
    savegame,
    quitdoom,
    main_end
} main_e;

static const menuitem_t MainMenu[]=
{
    {1,DMC_UI_M_NGAME,M_NewGame},
    {1,DMC_UI_M_OPTION,M_Options},
    {1,DMC_UI_M_LOADG,M_LoadGame},
    {1,DMC_UI_M_SAVEG,M_SaveGame},
    {1,DMC_UI_M_QUITG,M_QuitDOOM}
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

static const menuitem_t NewGameMenu[]=
{
    {1,DMC_UI_M_JKILL,	M_ChooseSkill},
    {1,DMC_UI_M_ROUGH,	M_ChooseSkill},
    {1,DMC_UI_M_HURT,	M_ChooseSkill},
    {1,DMC_UI_M_ULTRA,	M_ChooseSkill},
    {1,DMC_UI_M_NMARE,	M_ChooseSkill}
};


//
// OPTIONS MENU
//
enum
{
    endgame,
    messages,
    sound,
    usbdrive,
    benchmark,
    opt_end
} options_e;

static const menuitem_t OptionsMenu[]=
{
    {1,DMC_UI_M_ENDGAM,	M_EndGame},
    {1,DMC_UI_M_MESSG,	M_ChangeMessages},
    {2,-1,		M_ChangeSound},
    {1,-1,		M_UsbDrive},
    {1,-1,		M_Benchmark}
};


//
// LOAD GAME and SAVE GAME: the slots of SAVE.DAT
//
enum
{
    load1,
    load2,
    load3,
    load4,
    load_end
} load_e;

_Static_assert(load_end == DMC_SAVE_SLOTS, "a menu row per slot");

static const menuitem_t LoadMenu[]=
{
    {1,-1, M_LoadSelect},
    {1,-1, M_LoadSelect},
    {1,-1, M_LoadSelect},
    {1,-1, M_LoadSelect}
};

static const menuitem_t SaveMenu[]=
{
    {1,-1, M_SaveSelect},
    {1,-1, M_SaveSelect},
    {1,-1, M_SaveSelect},
    {1,-1, M_SaveSelect}
};


static const menu_t menus[] =
{
    [MENU_MAIN] = {main_end, -1, MainMenu, M_DrawMainMenu, 97,64},
    // no episode page: back to the main menu
    [MENU_NEW] = {newg_end, MENU_MAIN, NewGameMenu, M_DrawNewGame, 48,63},
    [MENU_OPTIONS] = {opt_end, MENU_MAIN, OptionsMenu, M_DrawOptions, 60,37},
    [MENU_LOAD] = {load_end, MENU_MAIN, LoadMenu, M_DrawLoad, 80,54},
    [MENU_SAVE] = {load_end, MENU_MAIN, SaveMenu, M_DrawSave, 80,54}
};


//
// Text in the hu_font, from a string of this overlay or of the image.
// Strings of the image are read through a window of 32 B, a line is
// measured with one walk and drawn with a second. scr_text of the finale
// lays out a whole text from one x, the messages are centered line by
// line, so the menu walks its lines here with the same glyphs
// (scr_glyph, port/render/r_screen.h).
//
typedef struct
{
    const char	*s;		// string of this overlay, or NULL
    int		id;		// else the string of the image
    uint32_t	off;		// where buf[0] is in it
    int		n;		// characters in buf
    char	buf[32];
} mtext_t;

static void M_Text(mtext_t *t, const char *s, int id)
{
    t->s = s;
    t->id = id;
    t->off = 0;
    t->n = 0;
}

// Character i of the text, 0 at its end
static int M_Char(mtext_t *t, uint32_t i)
{
    if (t->s)
	return t->s[i];
    if (i < t->off || i >= t->off + t->n)
    {
	t->off = i;
	t->n = str_part(t->id, i, t->buf, sizeof(t->buf));
	if (!t->n)
	    return 0;
    }
    return t->buf[i - t->off];
}

//
// Find string width from hu_font chars: the line of t from i on, *end
// the index of its newline or end.
//
static int M_StringWidth(mtext_t *t, uint32_t i, uint32_t *end)
{
    int             w = 0;
    int             c;

    for (; (c = M_Char(t, i)) != 0 && c != '\n'; i++)
    {
	c = scr_glyph(c);
	if (c < 0)
	    w += 4;
	else
	    w += V_PatchWidth(HU_FONT(c));
    }
    *end = i;

    return w;
}


//
//      Write a string using the hu_font: the line of t from i on
//
static void
M_WriteText
( int		x,
  int		y,
  mtext_t*	t,
  uint32_t	i )
{
    int		w;
    int		c;
    int		cx;

    cx = x;

    for (; (c = M_Char(t, i)) != 0 && c != '\n'; i++)
    {
	c = scr_glyph(c);
	if (c < 0)
	{
	    cx += 4;
	    continue;
	}

	w = V_PatchWidth(HU_FONT(c));
	if (cx+w > SCREENWIDTH)
	    break;
	V_DrawPatchDirect(cx, y, HU_FONT(c));
	cx+=w;
    }
}

// A string of this overlay at x, y
static void M_WriteString(int x, int y, const char *s)
{
    mtext_t	t;

    M_Text(&t, s, 0);
    M_WriteText(x, y, &t, 0);
}




//
// M_DrawMainMenu
//
static void M_DrawMainMenu(void)
{
    V_DrawPatchDirect(94, 2, V_UIPatch(DMC_UI_M_DOOM));
}




//
// M_NewGame
//
static void M_DrawNewGame(void)
{
    V_DrawPatchDirect(96, 14, V_UIPatch(DMC_UI_M_NEWG));
    V_DrawPatchDirect(54, 38, V_UIPatch(DMC_UI_M_SKILL));
}

static void M_NewGame(int choice)
{
    M_SetupNextMenu(MENU_NEW);
}


// Nightmare respawns monsters, which the object store of the meter
// cannot do: a confirmed Nightmare gets a second message, then the skill
// page again. Returns 1 when the menu stays up.
static int M_VerifyNightmare(int key)
{
    if (key != key_menu_confirm)
	return 0;

    M_StartMessage(MSG_NONIGHTMARE, false);
    return 1;
}

static void M_ChooseSkill(int choice)
{
    if (choice == nightmare)
    {
	M_StartMessage(DMC_STR_NIGHTMARE, true);
	return;
    }

    G_DeferedInitNew(choice,1,1);
    M_ClearMenus ();
}



//
// M_Options
//
static const short msgNames[2] = {DMC_UI_M_MSGOFF,DMC_UI_M_MSGON};
static const char *const soundNames[3] = {"off", "low", "high"};

// A text item of the options at row i, its value at the column of the
// messages switch
static void M_OptionText(int i, const char *s, const char *value)
{
    int		y = menus[MENU_OPTIONS].y + LINEHEIGHT * i + 4;

    M_WriteString(menus[MENU_OPTIONS].x, y, s);
    if (value)
	M_WriteString(menus[MENU_OPTIONS].x + 120, y, value);
}

static void M_DrawOptions(void)
{
    V_DrawPatchDirect(108, 15, V_UIPatch(DMC_UI_M_OPTTTL));

    V_DrawPatchDirect(menus[MENU_OPTIONS].x + 120,
		      menus[MENU_OPTIONS].y + LINEHEIGHT * messages,
                      V_UIPatch(msgNames[showMessages]));

    M_OptionText(sound, "sound", soundNames[dmc_set.sound % 3]);
    M_OptionText(usbdrive, "usb drive", NULL);
    M_OptionText(benchmark, "benchmark", NULL);
}

static void M_Options(int choice)
{
    M_SetupNextMenu(MENU_OPTIONS);
}



//
//      Toggle messages on/off
//
static void M_ChangeMessages(int choice)
{
    showMessages = 1 - showMessages;
    dmc_set.msg_off = !showMessages;
    fe.flags |= FE_SETTINGS;

    if (!showMessages)
	players[consoleplayer].message = P_String(DMC_STR_MSGOFF);
    else
	players[consoleplayer].message = P_String(DMC_STR_MSGON);

    message_dontfuckwithme = true;
}


//
// Sound: off, low, high, round in both directions
//
static void M_ChangeSound(int choice)
{
    dmc_set.sound = (dmc_set.sound + (choice ? 1 : 2)) % 3;
    snd_volume(dmc_set.sound);
    fe.flags |= FE_SETTINGS;
}


//
// The data drive on USB and the benchmark end the game: the play loop
// of the meter runs them (fe.action)
//
static void M_UsbDrive(int choice)
{
    M_StartMessage(MSG_USB, true);
}

static void M_Benchmark(int choice)
{
    M_StartMessage(MSG_BENCH, true);
}

static void M_Action(int key, int action)
{
    if (key != key_menu_confirm)
	return;
    fe.action = action;
    M_ClearMenus ();
}


//
// M_EndGame
//
static void M_EndGameResponse(int key)
{
    if (key != key_menu_confirm)
	return;

    fe.last[fe.menu] = itemOn;
    M_ClearMenus ();
    D_StartTitle ();
}

static void M_EndGame(int choice)
{
    if (!usergame)
    {
	S_StartSound(NULL,sfx_oof);
	return;
    }

    M_StartMessage(DMC_STR_ENDGAME, true);
}


//
// M_ReadSaveStrings
// The slots that hold a save of this game into fe.saves, the page draws
// their descriptions. Returns 0 when SAVE.DAT is missing or not usable.
//
static int M_ReadSaveStrings(void)
{
    dmc_save_hdr_t	h;
    int		i;

    fe.saves = 0;
    if (G_SaveUsable())
	return 0;
    for (i = 0; i < load_end; i++)
	if (!G_SaveHeader(i, &h))
	    fe.saves |= 1 << i;
    return 1;
}


//
// Draw border for the savegame description
//
static void M_DrawSaveLoadBorder(int x,int y)
{
    int             i;

    V_DrawPatchDirect(x - 8, y + 7, V_UIPatch(DMC_UI_M_LSLEFT));

    for (i = 0;i < 24;i++)
    {
	V_DrawPatchDirect(x, y + 7, V_UIPatch(DMC_UI_M_LSCNTR));
	x += 8;
    }

    V_DrawPatchDirect(x, y + 7, V_UIPatch(DMC_UI_M_LSRGHT));
}


//
// M_LoadGame & Cie.
// The page of the slots, the description of a save from its header
//
static void M_DrawSlots(int pic)
{
    const menu_t	*m = &menus[MENU_LOAD];
    char		desc[sizeof(((dmc_save_hdr_t *)0)->desc) + 1];
    mtext_t		t;
    int			i;

    V_DrawPatchDirect(72, 28, V_UIPatch(pic));

    for (i = 0;i < load_end; i++)
    {
	M_DrawSaveLoadBorder(m->x,m->y+LINEHEIGHT*i);
	if (fe.saves & 1 << i)
	{
	    ext_read(DMC_F_SAVE, i * SAVE_SLOT_BYTES + offsetof(dmc_save_hdr_t, desc), desc,
		     sizeof(desc) - 1);
	    desc[sizeof(desc) - 1] = 0;
	    M_Text(&t, desc, 0);
	}
	else
	    M_Text(&t, NULL, DMC_STR_EMPTYSTRING);
	M_WriteText(m->x,m->y+LINEHEIGHT*i,&t,0);
    }
}

static void M_DrawLoad(void)
{
    M_DrawSlots(DMC_UI_M_LOADG);
}

static void M_DrawSave(void)
{
    M_DrawSlots(DMC_UI_M_SAVEG);
}


//
// User wants to load this game: only a whole one, the game goes on as
// it is otherwise
//
static void M_LoadSelect(int choice)
{
    dmc_save_hdr_t	h;

    // The check reads the save through the tic scratch, where a piece of
    // the menu may still be on its way to the LCD when the NetUpdate of
    // D_Display reads the key.
    R_PumpIdle();
    if (G_SaveCheck(choice, &h))
    {
	M_StartMessage(MSG_BADSAVE, false);
	return;
    }
    G_LoadGame (choice);
    M_ClearMenus ();
}

//
// Selected from DOOM menu
//
static void M_LoadGame (int choice)
{
    if (!M_ReadSaveStrings())
    {
	M_StartMessage(MSG_NOSAVE, false);
	return;
    }
    M_SetupNextMenu(MENU_LOAD);
}


//
// User wants to save: at once, the save describes itself
//
static void M_SaveSelect(int choice)
{
    G_SaveGame (choice);
    M_ClearMenus ();
}

//
// Selected from DOOM menu
//
static void M_SaveGame (int choice)
{
    if (!usergame)
    {
	M_StartMessage(DMC_STR_SAVEDEAD, false);
	return;
    }

    if (gamestate != GS_LEVEL)
	return;

    if (!M_ReadSaveStrings())
    {
	M_StartMessage(MSG_NOSAVE, false);
	return;
    }
    M_SetupNextMenu(MENU_SAVE);
}



//
// M_QuitDOOM
//
static const short quitsounds[8] =
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


// The settings changed while the menu was up go to the settings page,
// which is written with the interrupts off: no strip may be on its way
// to the LCD.
static void M_SaveSettings(void)
{
    if (!(fe.flags & FE_SETTINGS))
	return;
    fe.flags &= ~FE_SETTINGS;
    R_PumpIdle();
    settings_save(&dmc_set, sizeof(dmc_set), SET_VERSION);
}

static void M_QuitResponse(int key)
{
    if (key != key_menu_confirm)
	return;
    M_SaveSettings();
    S_StartSound(NULL,quitsounds[(gametic>>2)&7]);
    I_WaitVBL(105);
    I_Quit ();
}


static void M_QuitDOOM(int choice)
{
    // QUITMSG, QUITMSG1 ... QUITMSG7 are consecutive strings
    fe.msgarg = gametic % NUM_QUITMESSAGES;
    M_StartMessage(DMC_STR_QUITMSG, true);
}


// The answer to the message up. Returns 1 when the menu stays up.
static int M_MessageResponse(int key)
{
    switch (fe.msg)
    {
      case DMC_STR_NIGHTMARE:
	return M_VerifyNightmare(key);
      case DMC_STR_ENDGAME:
	M_EndGameResponse(key);
	break;
      case DMC_STR_QUITMSG:
	M_QuitResponse(key);
	break;
      case MSG_USB:
	M_Action(key, FE_ACT_USB);
	break;
      case MSG_BENCH:
	M_Action(key, FE_ACT_BENCH);
	break;
      case MSG_NONIGHTMARE:
	return 1;
    }
    return 0;
}




//
//      Menu Functions
//

// The panel starts again with the next page drawn, unless no frame has
// drawn the picture under the panel of the menu closed last: then the
// new panel covers that one too
static void M_PanelReset(void)
{
    if (!(fe.flags & FE_PANEL))
    {
	fe.px0 = fe.py0 = 255;
	fe.px1 = fe.py1 = 0;
    }
#ifdef DMC_STATS
    // the host poisons the compose buffer in the tic scratch, the
    // front-end scratch keeps the state of the screen under the menu
    memset((byte *) &dmc_tic + sizeof(fe_scr_t), 0xa5, sizeof(dmc_tic) - sizeof(fe_scr_t));
#endif
}

static void
M_StartMessage
( int		msg,
  boolean	input )
{
    if (!menuactive)
	M_PanelReset();
    messageLastMenuActive = menuactive;
    messageToPrint = 1;
    fe.msg = msg;
    messageNeedsInput = input;
    menuactive = true;
    return;
}


//
// CONTROL PANEL
//

//
// M_OvlResponder
//
boolean M_OvlResponder (event_t* ev)
{
    int             key;

    // key is the key pressed

    key = -1;

    if (ev->type == ev_keydown)
    {
	key = ev->data1;
    }

    if (key == -1)
	return false;

    // Take care of any messages that need input
    if (messageToPrint)
    {
	if (messageNeedsInput)
        {
            if (key != ' ' && key != KEY_ESCAPE
             && key != key_menu_confirm && key != key_menu_abort)
            {
                return false;
            }
	}

	menuactive = messageLastMenuActive;
	messageToPrint = 0;
	if (!M_MessageResponse(key))
	    M_ClearMenus ();
	S_StartSound(NULL,sfx_swtchx);
	return true;
    }

    // F-Keys: the meter sends F10 alone
    if (!menuactive)
    {
        if (key == key_menu_quit)     // Quit DOOM
        {
	    S_StartSound(NULL,sfx_swtchn);
	    M_QuitDOOM(0);
	    return true;
        }
    }

    // Pop-up menu?
    if (!menuactive)
    {
	if (key == key_menu_activate)
	{
	    M_OvlOpen ();
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
	}
	return false;
    }

    // Keys usable within menu

    if (key == key_menu_down)
    {
        // Move down to next item

        do
	{
	    if (itemOn+1 > currentMenu->numitems-1)
		itemOn = 0;
	    else itemOn++;
	    S_StartSound(NULL,sfx_pstop);
	} while(currentMenu->menuitems[itemOn].status==-1);

	return true;
    }
    else if (key == key_menu_up)
    {
        // Move back up to previous item

	do
	{
	    if (!itemOn)
		itemOn = currentMenu->numitems-1;
	    else itemOn--;
	    S_StartSound(NULL,sfx_pstop);
	} while(currentMenu->menuitems[itemOn].status==-1);

	return true;
    }
    else if (key == key_menu_left)
    {
        // Slide slider left

	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status == 2)
	{
	    S_StartSound(NULL,sfx_stnmov);
	    currentMenu->menuitems[itemOn].routine(0);
	}
	return true;
    }
    else if (key == key_menu_right)
    {
        // Slide slider right

	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status == 2)
	{
	    S_StartSound(NULL,sfx_stnmov);
	    currentMenu->menuitems[itemOn].routine(1);
	}
	return true;
    }
    else if (key == key_menu_forward)
    {
        // Activate menu item

	// the empty slots of the load page cannot be chosen (vanilla's
	// status 0)
	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status &&
	    (fe.menu != MENU_LOAD || fe.saves & 1 << itemOn))
	{
	    fe.last[fe.menu] = itemOn;
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
    }
    else if (key == key_menu_activate)
    {
        // Deactivate menu

	fe.last[fe.menu] = itemOn;
	M_ClearMenus ();
	S_StartSound(NULL,sfx_swtchx);
	return true;
    }
    else if (key == key_menu_back)
    {
        // Go back to previous menu

	fe.last[fe.menu] = itemOn;
	if (currentMenu->prevMenu >= 0)
	{
	    M_SetupNextMenu(currentMenu->prevMenu);
	    S_StartSound(NULL,sfx_swtchn);
	}
	return true;
    }

    return false;
}



//
// M_OvlOpen (M_StartControlPanel)
//
void M_OvlOpen (void)
{
    // intro might call this repeatedly
    if (menuactive)
	return;

    M_PanelReset();
    menuactive = 1;
    M_SetupNextMenu(MENU_MAIN);         // JDC
}

//
// The page or the message of M_Drawer
//

// Lines of part k of the message from y on, each centered, drawn when
// draw is set. Returns the y below them.
static int M_MessagePart(int k, int y, int draw)
{
    mtext_t	t;
    uint32_t	i;
    uint32_t	end;
    int		w;

    if (fe.msg >= MSG_OWN)
	M_Text(&t, ownmessages[fe.msg - MSG_OWN], 0);
    else if (fe.msg != DMC_STR_QUITMSG)
	M_Text(&t, NULL, fe.msg);
    else
	M_Text(&t, NULL, k ? DMC_STR_DOSY : DMC_STR_QUITMSG + fe.msgarg);

    for (i = 0; ; i = end + 1)
    {
	w = M_StringWidth(&t, i, &end);
	if (draw)
	    M_WriteText(SCREENWIDTH/2 - w/2, y, &t, i);
	y += V_PatchHeight(HU_FONT(0));
	if (!M_Char(&t, end))
	    return y;
    }
}

static void M_DrawPage (void)
{
    short		x;
    short		y;
    unsigned int	i;
    unsigned int	max;
    int			parts;
    int			k;

    // Horiz. & Vertically center string and print it.
    if (messageToPrint)
    {
	parts = fe.msg == DMC_STR_QUITMSG ? 2 : 1;
	// the parts with an empty line between them
	y = 0;
	for (k = 0; k < parts; k++)
	    y = M_MessagePart(k, y + (k ? V_PatchHeight(HU_FONT(0)) : 0), 0);
	y = SCREENHEIGHT/2 - y/2;
	for (k = 0; k < parts; k++)
	    y = M_MessagePart(k, y + (k ? V_PatchHeight(HU_FONT(0)) : 0), 1);
	return;
    }

    if (currentMenu->routine)
	currentMenu->routine();         // call Draw routine

    // DRAW MENU
    x = currentMenu->x;
    y = currentMenu->y;
    max = currentMenu->numitems;

    for (i=0;i<max;i++)
    {
	if (currentMenu->menuitems[i].pic >= 0)
	{
	    V_DrawPatchDirect (x, y, V_UIPatch(currentMenu->menuitems[i].pic));
	}
	y += LINEHEIGHT;
    }


    // DRAW SKULL
    V_DrawPatchDirect(x + SKULLXOFF, currentMenu->y - 5 + itemOn*LINEHEIGHT,
		      V_UIPatch(skullName[whichSkull]));
}

//
// M_OvlDrawer
// The page on the panel, which grows to the box of the page first when
// it does not hold it
//
void M_OvlDrawer (void)
{
    scr_win_t	panel;
    scr_win_t	box;

    for (;;)
    {
	panel.x0 = 2 * fe.px0;
	panel.x1 = 2 * fe.px1 + 1;
	panel.y0 = fe.py0;
	panel.y1 = fe.py1;
	if (R_ScreenDraw(FE_SCR_MENU, &panel, M_DrawPage, &box))
	    return;
	// whole pairs of columns, rows by 4 as the LCD windows want them
	if (box.x0 < panel.x0)
	    fe.px0 = box.x0 / 2;
	if (box.x1 > panel.x1)
	    fe.px1 = box.x1 / 2;
	if (box.y0 < panel.y0)
	    fe.py0 = box.y0 & ~3;
	if (box.y1 > panel.y1)
	    fe.py1 = box.y1 | 3;
    }
}


//
// M_ClearMenus
// The picture under the menu is composed whole again by the next frame,
// its panel stays on the LCD until then
//
static void M_ClearMenus (void)
{
    menuactive = 0;
    fe.flags &= ~FE_HASHED;
    fe.flags |= FE_PANEL;
    M_SaveSettings();
}




//
// M_SetupNextMenu
//
static void M_SetupNextMenu(int menu)
{
    fe.menu = menu;
    itemOn = fe.last[menu];
}
