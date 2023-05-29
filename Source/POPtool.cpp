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

#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>
#include "Misc.h"
#include "Vars.h"
#include "DAT.h"
#include "DAT-Formats.h"
#include "Repack.h"

enum
{
	MODE_NOTHING,
	MODE_EXTRACTALLFILES,
	MODE_EXTRACTDAT,
	MODE_REPACKDAT,
};

enum
{
	GAME_UNKNOWN,
	GAME_POP1,
	GAME_POP2,
};

static int game = GAME_UNKNOWN;

void HelpText()
{
	printf("Prince of Persia DAT tool v0.1 (by patreon/FluffyQuack)\n\n");
	printf("usage: POPtool [options]\n");
	printf("  -x [dat]		Unpack DAT container file\n");
	printf("  -r [dat]		Recreate DAT container\n");
	printf("  -all			Extract all DAT containers for a game\n");
	printf("  -POP1			Define POP1 as active game\n");
	printf("  -POP2			Define POP2 as active game\n");
}

int _tmain(int argc, _TCHAR *argv[])
{
	//Defaults
	int mode = MODE_NOTHING;
	bool createdNewHashList = 0;

	//Process command line arguments
	int i = 1, strcount = 0;
	char *str1 = 0, *str2 = 0;
	while(argc > i)
	{
		if(argv[i][0] == '-')
		{
			if(_stricmp(argv[i], "-all") == 0)
				mode = MODE_EXTRACTALLFILES;
			else if(_stricmp(argv[i], "-pop1") == 0)
				game = GAME_POP1;
			else if(_stricmp(argv[i], "-pop2") == 0)
				game = GAME_POP2;
			else if(_stricmp(argv[i], "-x") == 0)
				mode = MODE_EXTRACTDAT;
			else if(_stricmp(argv[i], "-r") == 0)
				mode = MODE_REPACKDAT;
		}
		else
		{
			if(strcount == 0)
				str1 = argv[i];
			else if(strcount == 1)
				str2 = argv[i];
			strcount++;
		}
		i++;
	}

	if(mode == MODE_EXTRACTDAT)
	{
		if(str1 && game == GAME_POP1)
		{
			Prince_ExtractDAT(str1);
		}
		else if(str1 && game == GAME_POP2)
		{
			Prince_ExtractDATv2(str1);
		}
		else
			StatusUpdate("Warning: Missing filepath or game definition\n");
	}
	else if(mode == MODE_REPACKDAT)
	{
		if(str1 && game == GAME_POP1)
		{
			StatusUpdate("Warning: POP1 repacking not supported\n");
		}
		else if(str1 && game == GAME_POP2)
		{
			Prince_RepackDATv2(str1);
		}
		else
			StatusUpdate("Warning: Missing filepath or game definition\n");
	}
	else if(mode == MODE_EXTRACTALLFILES)
	{
		if(game == GAME_POP1)
		{
			Prince_ExtractDAT("KID.DAT");
			Prince_ExtractDAT("VDUNGEON.DAT");
			Prince_ExtractDAT("FAT.DAT");
			Prince_ExtractDAT("VPALACE.DAT");
			Prince_ExtractDAT("GUARD1.DAT");
			Prince_ExtractDAT("GUARD2.DAT");
			{
				unsigned char *palData = 0;
				unsigned int palDataSize = 0;
				if(Prince_OpenDAT("PRINCE.DAT"))
				{
					Prince_LoadEntryFromDAT(&palData, &palDataSize, -1, 10);
					Prince_CloseDAT();
				}
				if(palData)
				{
					Prince_ExtractDAT("GUARD.DAT", palData, palDataSize, POP1_DATFORMAT_MULTIPAL);
				}
				delete[]palData;
			}
			Prince_ExtractDAT("PRINCE.DAT");
			Prince_ExtractDAT("PV.DAT");
			Prince_ExtractDAT("SHADOW.DAT");
			Prince_ExtractDAT("SKEL.DAT");
			Prince_ExtractDAT("TITLE.DAT");
			Prince_ExtractDAT("VIZIER.DAT");
		}
		else if(game == GAME_POP2)
		{
			//First extract everything using default settings
			Prince_ExtractDATv2("BIRD.DAT"); //Everything uses correct palette automatically
			Prince_ExtractDATv2("CAVERNS.DAT");
			Prince_ExtractDATv2("DESERT.DAT"); //First image looks a bit weird, but otherwise automatic palette is correct
			Prince_ExtractDATv2("DIGISND.DAT");
			Prince_ExtractDATv2("FINAL.DAT");
			Prince_ExtractDATv2("FLAME.DAT"); //Everything uses correct palette automatically
			Prince_ExtractDATv2("GUARD.DAT"); //Everything uses correct palette automatically. However, I believe there are variants of this sprite depending on the stage
			Prince_ExtractDATv2("HEAD.DAT"); //Everything uses correct palette automatically with maybe the exception of the last image that looks a bit weird
			Prince_ExtractDATv2("IBMSND.DAT");
			Prince_ExtractDATv2("JINNE.DAT");
			Prince_ExtractDATv2("KID.DAT"); //Everything uses correct palette automatically, maybe with the exception of the magical-esque animation with id of around 24892
			Prince_ExtractDATv2("MIDISND.DAT");
			Prince_ExtractDATv2("NIS.DAT");
			Prince_ExtractDATv2("NIS3VC.DAT");
			Prince_ExtractDATv2("NISDIGI.DAT");
			Prince_ExtractDATv2("NISIBM.DAT");
			Prince_ExtractDATv2("NISMIDI.DAT");
			Prince_ExtractDATv2("PRINCE.DAT");
			Prince_ExtractDATv2("ROOFTOPS.DAT");
			Prince_ExtractDATv2("RUINS.DAT");
			Prince_ExtractDATv2("SEQUENCE.DAT");
			Prince_ExtractDATv2("\\SEQUENCE.DAT");
			Prince_ExtractDATv2("SKELETON.DAT"); //Everything uses correct palette automatically
			Prince_ExtractDATv2("TEMPLE.DAT");
			Prince_ExtractDATv2("TRANS.DAT"); //Causes a heap corruption error message

			//Extract images with correct palettes
			{
				Prince_OpenDATv2("CAVERNS.DAT");
				unsigned char *pal1Data = 0; unsigned int pal1DataSize = 0;
				Prince_LoadEntryFromDATv2(&pal1Data, &pal1DataSize, POP2_DATFORMAT_SVGA_PALETTE, -1, 25303);
				Prince_CloseDAT();

				Prince_ExtractDATv2("CAVERNS.DAT", 0, 0, 0, 3501, 4059); //These images use correct palette automatically
				Prince_ExtractDATv2("CAVERNS.DAT", 0, 0, 0, 4225, 4235); //Wooden bridge with loose steps tied by rope room (TODO: Which palette should we use? Or is this correct even though there are weird red pixels?)
				Prince_ExtractDATv2("CAVERNS.DAT", pal1Data, pal1DataSize, POP2_DATFORMAT_SVGA_PALETTE, 25303, 25312); //Magic carpet
				if(pal1Data) delete[]pal1Data;
			}
			{
				Prince_OpenDATv2("FINAL.DAT");
				unsigned char *pal1Data = 0;
				unsigned int pal1DataSize = 0;
				Prince_LoadEntryFromDATv2(&pal1Data, &pal1DataSize, POP2_DATFORMAT_TGA_PALETTE, -1, 25000);
				Prince_CloseDAT();

				Prince_ExtractDATv2("FINAL.DAT", 0, 0, 0, 25001, 26052); //These images use correct palette automatically //TODO: Some of them are wrong
				Prince_ExtractDATv2("FINAL.DAT", pal1Data, pal1DataSize, POP2_DATFORMAT_TGA_PALETTE, 24902, 25390); //Not sure if these are correct
				if(pal1Data) delete[]pal1Data;
			}
			{
				Prince_OpenDATv2("NIS.DAT");
				unsigned char *pal1Data = 0; unsigned int pal1DataSize = 0;
				unsigned char *pal2Data = 0; unsigned int pal2DataSize = 0;
				unsigned char *pal3Data = 0; unsigned int pal3DataSize = 0;
				unsigned char *pal4Data = 0; unsigned int pal4DataSize = 0;
				Prince_LoadEntryFromDATv2(&pal1Data, &pal1DataSize, POP2_DATFORMAT_TGA_PALETTE, -1, 27001);
				Prince_LoadEntryFromDATv2(&pal2Data, &pal2DataSize, POP2_DATFORMAT_TGA_PALETTE, -1, 28001);
				Prince_LoadEntryFromDATv2(&pal3Data, &pal3DataSize, POP2_DATFORMAT_TGA_PALETTE, -1, 29001);
				Prince_LoadEntryFromDATv2(&pal4Data, &pal4DataSize, POP2_DATFORMAT_TGA_PALETTE, -1, 30001);
				Prince_CloseDAT();

				Prince_ExtractDATv2("NIS.DAT", 0, 0, 0, 3501, 4189); //These images use correct palette automatically (maybe a few are wrong, but they look right at first glance)
				Prince_ExtractDATv2("NIS.DAT", pal1Data, pal1DataSize, POP2_DATFORMAT_TGA_PALETTE, 27004, 27045); //Some are wrong
				Prince_ExtractDATv2("NIS.DAT", pal2Data, pal2DataSize, POP2_DATFORMAT_TGA_PALETTE, 28002, 28017); //Some are wrong
				Prince_ExtractDATv2("NIS.DAT", pal3Data, pal3DataSize, POP2_DATFORMAT_TGA_PALETTE, 29002, 29023); //Some are wrong
				Prince_ExtractDATv2("NIS.DAT", pal4Data, pal4DataSize, POP2_DATFORMAT_TGA_PALETTE, 30002, 30044); //Some are wrong
				if(pal1Data) delete[]pal1Data;
				if(pal2Data) delete[]pal2Data;
				if(pal3Data) delete[]pal3Data;
				if(pal4Data) delete[]pal4Data;
			}
			{
				Prince_OpenDATv2("PRINCE.DAT");
				unsigned char *pal1Data = 0; unsigned int pal1DataSize = 0;
				unsigned char *pal2Data = 0; unsigned int pal2DataSize = 0;
				unsigned char *pal3Data = 0; unsigned int pal3DataSize = 0;
				unsigned char *pal4Data = 0; unsigned int pal4DataSize = 0;
				Prince_LoadEntryFromDATv2(&pal1Data, &pal1DataSize, POP2_DATFORMAT_SHAPE_PALETTE, -1, 1000);
				Prince_LoadEntryFromDATv2(&pal2Data, &pal2DataSize, POP2_DATFORMAT_SHAPE_PALETTE, -1, 3000);
				Prince_LoadEntryFromDATv2(&pal3Data, &pal3DataSize, POP2_DATFORMAT_SHAPE_PALETTE, -1, 8000);
				Prince_LoadEntryFromDATv2(&pal4Data, &pal4DataSize, POP2_DATFORMAT_SVGA_PALETTE, -1, 10);
				Prince_CloseDAT();

				Prince_ExtractDATv2("PRINCE.DAT", pal1Data, pal1DataSize, POP2_DATFORMAT_SHAPE_PALETTE, 1001, 1246); //Sword
				Prince_ExtractDATv2("PRINCE.DAT", pal2Data, pal2DataSize, POP2_DATFORMAT_SHAPE_PALETTE, 3001, 3035); //Potions
				Prince_ExtractDATv2("PRINCE.DAT", pal3Data, pal3DataSize, POP2_DATFORMAT_SHAPE_PALETTE, 8001, 8013); //Copy protection
				Prince_ExtractDATv2("PRINCE.DAT", pal4Data, pal4DataSize, POP2_DATFORMAT_SVGA_PALETTE, 25456, 25458); //Horse statue (wrong palette)
				if(pal1Data) delete[]pal1Data;
				if(pal2Data) delete[]pal2Data;
				if(pal3Data) delete[]pal3Data;
				if(pal4Data) delete[]pal4Data;
			}
			{
				Prince_OpenDATv2("ROOFTOPS.DAT");
				unsigned char *pal1Data = 0; unsigned int pal1DataSize = 0;
				Prince_LoadEntryFromDATv2(&pal1Data, &pal1DataSize, POP2_DATFORMAT_SVGA_PALETTE, -1, 3500);
				Prince_CloseDAT();

				Prince_ExtractDATv2("ROOFTOPS.DAT", 0, 0, 0, 3501, 4122); //These images use correct palette automatically (the first one looks weird but I think it's also unused)
				Prince_ExtractDATv2("ROOFTOPS.DAT", pal1Data, pal1DataSize, POP2_DATFORMAT_SVGA_PALETTE, 4123, 4352); //These are still wrong. Maybe relying on a palette from another file?
				if(pal1Data) delete[]pal1Data;
			}
			{
				Prince_ExtractDATv2("RUINS.DAT", 0, 0, 0, 3501, 4267); //These images use correct palette automatically
				Prince_ExtractDATv2("RUINS.DAT", 0, 0, 0, 4600, 4602); //I think these are the starting screens for Ruins. I think palette is from another file, though
			}
			{
				Prince_OpenDATv2("TEMPLE.DAT");
				unsigned char *pal1Data = 0; unsigned int pal1DataSize = 0;
				Prince_LoadEntryFromDATv2(&pal1Data, &pal1DataSize, POP2_DATFORMAT_SVGA_PALETTE, -1, 4075);
				Prince_CloseDAT();

				Prince_ExtractDATv2("TEMPLE.DAT", 0, 0, 0, 3501, 4026);
				Prince_ExtractDATv2("TEMPLE.DAT", 0, 0, 0, 4075, 4077);
				Prince_ExtractDATv2("TEMPLE.DAT", pal1Data, pal1DataSize, POP2_DATFORMAT_SVGA_PALETTE, 4078, 4105);
				Prince_ExtractDATv2("TEMPLE.DAT", 0, 0, 0, 4106, 4106);
				if(pal1Data) delete[]pal1Data;
			}
			{
				Prince_OpenDATv2("TRANS.DAT");
				unsigned char *pal1Data = 0; unsigned int pal1DataSize = 0;
				unsigned char *pal2Data = 0; unsigned int pal2DataSize = 0;
				unsigned char *pal4Data = 0; unsigned int pal4DataSize = 0;
				Prince_LoadEntryFromDATv2(&pal1Data, &pal1DataSize, POP2_DATFORMAT_SHAPE_PALETTE, -1, 25000);
				Prince_LoadEntryFromDATv2(&pal2Data, &pal2DataSize, POP2_DATFORMAT_TGA_PALETTE, -1, 4208);
				Prince_LoadEntryFromDATv2(&pal4Data, &pal4DataSize, POP2_DATFORMAT_TGA_PALETTE, -1, 25001);
				Prince_CloseDAT();

				Prince_ExtractDATv2("TRANS.DAT", pal2Data, pal2DataSize, POP2_DATFORMAT_SHAPE_PALETTE, 4208, 4209); //Background for cutscene with "come to me" lady
				Prince_ExtractDATv2("TRANS.DAT", pal2Data, pal2DataSize, POP2_DATFORMAT_SHAPE_PALETTE, 4210, 4213); //"Come to me" lady sprite
				Prince_ExtractDATv2("TRANS.DAT", pal1Data, pal1DataSize, POP2_DATFORMAT_SHAPE_PALETTE, 25235, 25283); //Player sprite frames
				Prince_ExtractDATv2("TRANS.DAT", pal1Data, pal1DataSize, POP2_DATFORMAT_SHAPE_PALETTE, 25284, 25298); //More player sprite frames. Used for cutscenes?
				Prince_ExtractDATv2("TRANS.DAT", pal1Data, pal1DataSize, POP2_DATFORMAT_SVGA_PALETTE, 25299, 25302); //Potion
				Prince_ExtractDATv2("TRANS.DAT", pal4Data, pal4DataSize, POP2_DATFORMAT_SVGA_PALETTE, 25312, 25313); //Game logos
				Prince_ExtractDATv2("TRANS.DAT", pal4Data, pal4DataSize, POP2_DATFORMAT_SVGA_PALETTE, 25316, 25400); //Potion
				Prince_ExtractDATv2("TRANS.DAT", pal4Data, pal4DataSize, POP2_DATFORMAT_SVGA_PALETTE, 25401, 25455); //Player riding horse in cutscene
				if(pal1Data) delete[]pal1Data;
				if(pal2Data) delete[]pal2Data;
				if(pal4Data) delete[]pal4Data;
			}
		}
		else
			StatusUpdate("Warning: Missing game definition\n");
	}

	if(mode == MODE_NOTHING)
		HelpText();

	return 1;
}