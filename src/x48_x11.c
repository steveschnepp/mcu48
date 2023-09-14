/*
 *  This file is part of x48, an emulator of the HP-48sx Calculator.
 *  Copyright (C) 1994  Eddie C. Dost  (ecd@dressler.de)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Log: x48_x11.c,v $
 * Revision 1.13  1995/01/11  18:20:01  ecd
 * major update to support HP48 G/GX
 *
 * Revision 1.12  1994/12/08  22:17:24  ecd
 * display and menu images now correctly drawn according to disp.lines
 *
 * Revision 1.11  1994/12/07  20:20:50  ecd
 * better handling of resources
 *
 * Revision 1.10  1994/11/28  02:00:51  ecd
 * implemented WM_SAVE_YOURSELF protocol.
 * added support for mono and gray in color_t.
 * added support for all possible Visualclasses.
 * changed handling of KeyPress and KeyRelease.
 * added color icon stuff.
 * added support for contrast changes (ON_-, ON_+)
 * read in all those Xresources before running off.
 * use own icon window, no name-decor on icon.
 * show state of x48 in the icon's display
 * added support for setting the window title with the connections.
 *
 * Revision 1.9  1994/11/04  03:42:34  ecd
 * changed includes
 *
 * Revision 1.8  1994/11/02  14:44:28  ecd
 * works on machines that don't support backing store
 *
 * Revision 1.7  1994/10/09  20:32:02  ecd
 * changed refresh_display to support bit offset.
 *
 * Revision 1.6  1994/10/06  16:30:05  ecd
 * added XShm - Extension stuff
 *
 * Revision 1.5  1994/10/05  08:36:44  ecd
 * added backing_store = Always for subwindows
 *
 * Revision 1.4  1994/09/30  12:37:09  ecd
 * added support for interrupt detection in GetEvent,
 * faster display updates,
 * update display window only when mapped.
 *
 * Revision 1.3  1994/09/18  22:47:20  ecd
 * added version information
 *
 * Revision 1.2  1994/09/18  15:29:22  ecd
 * started Real Time support
 *
 * Revision 1.1  1994/09/13  15:05:05  ecd
 * Initial revision
 *
 * $Id: x48_x11.c,v 1.13 1995/01/11 18:20:01 ecd Exp ecd $
 */

#include "global.h"

#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "buttons.h"
#include "hp.h"
#include "icon.h"
#include "small.h"
#include "x48_x11.h"

#include "constants.h"
#include "device.h"
#include "errors.h"
#include "hp48.h"
#include "options.h"
#include "resources.h"
#include "romio.h"

static char *defaults[] = {
#include "X48.ad.h"
    0};

extern int saved_argc;
extern char **saved_argv;

int screen;
unsigned int depth;
disp_t disp;
static int last_icon_state = -1;

int dynamic_color;
int direct_color;
int does_backing_store;
int color_mode;
int icon_color_mode;

#if 0
#define DEBUG_XEVENT 1
#define DEBUG_BUTTONS 1
#define DEBUG_FOCUS 1
#define DEBUG_BACKING_STORE 1
#define DEBUG_SHM 1
#endif

typedef struct keypad_t {
  unsigned int width;
  unsigned int height;
} keypad_t;

keypad_t keypad;
color_t *colors;

typedef struct button_t {

  char *name;
  short pressed;
  short extra;

  int code;
  int x, y;
  unsigned int w, h;

  int lc;
  char *label;
  short font_size;
  unsigned int lw, lh;
  unsigned char *lb;

  char *letter;

  char *left;
  short is_menu;
  char *right;
  char *sub;

} button_t;

#define BUTTON_A 0
#define BUTTON_B 1
#define BUTTON_C 2
#define BUTTON_D 3
#define BUTTON_E 4
#define BUTTON_F 5

#define BUTTON_MTH 6
#define BUTTON_PRG 7
#define BUTTON_CST 8
#define BUTTON_VAR 9
#define BUTTON_UP 10
#define BUTTON_NXT 11

#define BUTTON_COLON 12
#define BUTTON_STO 13
#define BUTTON_EVAL 14
#define BUTTON_LEFT 15
#define BUTTON_DOWN 16
#define BUTTON_RIGHT 17

#define BUTTON_SIN 18
#define BUTTON_COS 19
#define BUTTON_TAN 20
#define BUTTON_SQRT 21
#define BUTTON_POWER 22
#define BUTTON_INV 23

#define BUTTON_ENTER 24
#define BUTTON_NEG 25
#define BUTTON_EEX 26
#define BUTTON_DEL 27
#define BUTTON_BS 28

#define BUTTON_ALPHA 29
#define BUTTON_7 30
#define BUTTON_8 31
#define BUTTON_9 32
#define BUTTON_DIV 33

#define BUTTON_SHL 34
#define BUTTON_4 35
#define BUTTON_5 36
#define BUTTON_6 37
#define BUTTON_MUL 38

#define BUTTON_SHR 39
#define BUTTON_1 40
#define BUTTON_2 41
#define BUTTON_3 42
#define BUTTON_MINUS 43

#define BUTTON_ON 44
#define BUTTON_0 45
#define BUTTON_PERIOD 46
#define BUTTON_SPC 47
#define BUTTON_PLUS 48

#define LAST_BUTTON 48

button_t *buttons;

typedef struct icon_t {
  unsigned int w;
  unsigned int h;
  int c;
  unsigned char *bits;
} icon_map_t;

#define ICON_MAP 0
#define ON_MAP 1
#define DISP_MAP 2
#define FIRST_MAP 3
#define LAST_MAP 9

icon_map_t *icon_maps;

icon_map_t icon_maps_sx[] = {
    {hp48_icon_width, hp48_icon_height, BLACK, hp48_icon_bits},
    {hp48_on_width, hp48_on_height, PIXEL, hp48_on_bits},
    {hp48_disp_width, hp48_disp_height, LCD, hp48_disp_bits},
    {hp48_top_width, hp48_top_height, DISP_PAD, hp48_top_bits},
    {hp48_bottom_width, hp48_bottom_height, PAD, hp48_bottom_bits},
    {hp48_logo_width, hp48_logo_height, LOGO, hp48_logo_bits},
    {hp48_text_width, hp48_text_height, LABEL, hp48_text_bits},
    {hp48_keys_width, hp48_keys_height, BLACK, hp48_keys_bits},
    {hp48_orange_width, hp48_orange_height, LEFT, hp48_orange_bits},
    {hp48_blue_width, hp48_blue_height, RIGHT, hp48_blue_bits}};

icon_map_t icon_maps_gx[] = {
    {hp48_icon_width, hp48_icon_height, BLACK, hp48_icon_bits},
    {hp48_on_width, hp48_on_height, PIXEL, hp48_on_bits},
    {hp48_disp_width, hp48_disp_height, LCD, hp48_disp_bits},
    {hp48_top_gx_width, hp48_top_gx_height, DISP_PAD, hp48_top_gx_bits},
    {hp48_bottom_width, hp48_bottom_height, PAD, hp48_bottom_bits},
    {hp48_logo_gx_width, hp48_logo_gx_height, LOGO, hp48_logo_gx_bits},
    {hp48_text_gx_width, hp48_text_gx_height, LABEL, hp48_text_gx_bits},
    {hp48_keys_width, hp48_keys_height, BLACK, hp48_keys_bits},
    {hp48_orange_width, hp48_orange_height, LEFT, hp48_orange_bits},
    {hp48_green_gx_width, hp48_green_gx_height, RIGHT, hp48_green_gx_bits}};

#define KEYBOARD_HEIGHT (buttons[LAST_BUTTON].y + buttons[LAST_BUTTON].h)
#define KEYBOARD_WIDTH (buttons[LAST_BUTTON].x + buttons[LAST_BUTTON].w)

#define TOP_SKIP 65
#define SIDE_SKIP 20
#define BOTTOM_SKIP 25
#define DISP_KBD_SKIP 65

#define DISPLAY_WIDTH (264 + 8)
#define DISPLAY_HEIGHT (128 + 16 + 8)
#define DISPLAY_OFFSET_X (SIDE_SKIP + (286 - DISPLAY_WIDTH) / 2)
#define DISPLAY_OFFSET_Y TOP_SKIP

#define DISP_FRAME 8

#define KEYBOARD_OFFSET_X SIDE_SKIP
#define KEYBOARD_OFFSET_Y (TOP_SKIP + DISPLAY_HEIGHT + DISP_KBD_SKIP)

int InitDisplay (int argc, char **argv) {}
int CreateWindows (int argc, char **argv) {}
int GetEvent (void) {}

void adjust_contrast (int contrast) {}
void refresh_icon (void) {}

void ShowConnections (char *w, char *i) {}

void exit_x48 (int tell_x11) {}
