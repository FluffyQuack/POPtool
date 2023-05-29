/*
Copyright (C) 2023 FluffyQuack

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

//Various unofficial and official POP technical documentation: https://www.popot.org/documentation.php
//Official Apple II source code: https://github.com/jmechner/Prince-of-Persia-Apple-II
//DOS Prince of Persia reverse-engineered: https://github.com/NagyD/SDLPoP
//Disassembly of POP2: https://forum.princed.org/viewtopic.php?f=64&t=3424
//Tool for extracting assets (and converting some to other formats) from POP1 and POP2: https://github.com/NagyD/PR
//Information about POP2 Mac resources: https://forum.princed.org/viewtopic.php?f=70&t=4201
//Full-level collages of all POP1 levels: https://strategywiki.org/wiki/Prince_of_Persia/Level_1
//Full-level collages of all POP2 levels: https://www.oldgames.sk/game/prince-of-persia-2/download/9428/

#define PRINCE_AP_OPSIZE short

#define PRINCEMAXPALSIZE 576

enum
{
	P_DIR_LEFT = 0, //Prince of Persia assets default to characters facing left
	P_DIR_RIGHT = 1, //Right is always flipped with POP assets
};

//Animation Bytecode ops
enum
{
	PRINCE_AP_SHOWFRAME, //Show specific frame (short)
	PRINCE_AP_JUMPTO, //Jump to animation start (short/short)
	PRINCE_AP_JUMPTO_IFFEATHER, //Jump to animation start (same as above, but only happens if player has feather effect on) (short/short)
	PRINCE_AP_MOVEX, //Move entity's X coordinate (short)
	PRINCE_AP_MOVEY, //Move entity's Y coordinate (short)
	PRINCE_AP_PLAYSOUND, //Play sound emitting from entity (short)
	PRINCE_AP_ACTION_STAND,
	PRINCE_AP_ACTION_RUN_JUMP,
	PRINCE_AP_ACTION_HANG_CLIMB,
	PRINCE_AP_ACTION_IN_MIDAIR,
	PRINCE_AP_ACTION_IN_FREEFALL,
	PRINCE_AP_ACTION_BUMPED,
	PRINCE_AP_ACTION_HANG_STRAIGHT,
	PRINCE_AP_ACTION_TURN,
	PRINCE_AP_ACTION_HURT,
	PRINCE_AP_SETFALL, //Not sure what this is. Takes 2 signed integers (short/short)
	PRINCE_AP_KNOCKDOWN,
	PRINCE_AP_KNOCKUP,
	PRINCE_AP_FLIP, //Flip entity
	PRINCE_AP_DIE, //Set entity's health to zero
	PRINCE_AP_MOVEDOWN,
	PRINCE_AP_MOVEUP,
	PRINCE_AP_GETITEM, // (short)
	PRINCE_AP_ENDLEVEL,

	//POP2-only OPs
	PRINCE_AP_UNKNOWN23, //Could this be loop final op?
	PRINCE_AP_UNKNOWN21, // (short)
	PRINCE_AP_UNKNOWN18,
	PRINCE_AP_UNKNOWN11, // (short)
	PRINCE_AP_UNKNOWN9, // (short/short)
	PRINCE_AP_DISAPPEAR,
	PRINCE_AP_ALIGNTOFLOOR,
	PRINCE_AP_SETPALETTE, // (short)
	PRINCE_AP_UP,
	PRINCE_AP_DOWN,
	PRINCE_AP_RANDOMBRANCH, // (short)

	PRINCE_AP_NUM
};


//Player states. List taken from SDL-PoP
enum
{
	PRINCE_PLAYERSTATE_STAND,
	PRINCE_PLAYERSTATE_RUN_JUMP,
	PRINCE_PLAYERSTATE_HANG_CLIMB,
	PRINCE_PLAYERSTATE_IN_MIDAIR,
	PRINCE_PLAYERSTATE_IN_FREEFALL,
	PRINCE_PLAYERSTATE_BUMPED,
	PRINCE_PLAYERSTATE_HANG_STRAIGHT,
	PRINCE_PLAYERSTATE_TURN,
	PRINCE_PLAYERSTATE_HURT,
};

struct princeAnim_s
{
	unsigned int byteCodeSize;
	unsigned char *byteCode;
	char *scriptName;
};

struct princeColour_s
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
};

struct princeGenericPalette_s
{
	princeColour_s colours[PRINCEMAXPALSIZE];
};

#pragma pack(push, 1)
struct datHeader_s
{
	unsigned int footerOffset;
	unsigned short footerSize; //Note: footerOffset + footerSize should match size of the DAT file
};

struct datFooter_s
{
	unsigned short entryCount;
};

struct datFooterEntry_s
{
	unsigned short id;
	unsigned long offset; //Offset in the DAT where the entry's data starts (note: an entry's data starts with a checksum byte)
	unsigned short size; //Size of the entry data in the DAT file (does not include checksum byte)
};

struct datFooterEntryV2_s
{
	unsigned short id;
	unsigned long offset; //Offset in the DAT where the entry's data starts (note: an entry's data starts with a checksum byte)
	unsigned short size; //Size of the entry data in the DAT file (does not include checksum byte)
	unsigned char flags[3]; //Maybe flag values? I noticed first byte was 64 and the others were 00 for most of the "shape" entrys in KID.DAT
};

struct datMasterIndex_s //Instead of a footer, we get this master index in POP2
{
	unsigned short footerCount;
};

struct datFooterHeader_s //Followed by POP2's "master index" we get definitions for various footers (the offsets point to footers that are identical to the ones in POP1)
{
	char magic[4];
	unsigned short footerOffset; //This offset counts from the position of masterIndex, not start of file
};
#pragma pack(pop)