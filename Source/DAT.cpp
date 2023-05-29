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

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>
#include "Misc.h"
#include "Vars.h"
#include "DAT.h"
#include "DAT-Formats.h"

//TODO: We should make it possible for the LoadEntry functions to return entry type. Might be useful when it comes to palettes

struct entryList_s
{
	int type;
	datFooterEntryV2_s *entries;
	unsigned short entryCount;
};

struct datContext_s
{
	FILE *file;
	entryList_s *entryLists;
	unsigned short entryListCount;
	unsigned long totalFileCount;
};

static datContext_s datContext = {0, 0, 0, 0};

static int DefineTypeBasedOnMagic(char *magic)
{
	if(memcmp(magic, "TSUC", 4) == 0) return POP2_DATFORMAT_CUSTOM;
	else if(memcmp(magic, "TNOF", 4) == 0) return POP2_DATFORMAT_FONT;
	else if(memcmp(magic, "MARF", 4) == 0) return POP2_DATFORMAT_FRAME;
	else if(memcmp(magic, "CLAP", 4) == 0) return POP2_DATFORMAT_CGA_PALETTE;
	else if(memcmp(magic, "SLAP", 4) == 0) return POP2_DATFORMAT_SVGA_PALETTE;
	else if(memcmp(magic, "TLAP", 4) == 0) return POP2_DATFORMAT_TGA_PALETTE;
	else if(memcmp(magic, "CEIP", 4) == 0) return POP2_DATFORMAT_PIECE;
	else if(memcmp(magic, "LSP\0", 4) == 0) return POP2_DATFORMAT_PSL;
	else if(memcmp(magic, "RCS\0", 4) == 0) return POP2_DATFORMAT_SCREEN;
	else if(memcmp(magic, "PAHS", 4) == 0) return POP2_DATFORMAT_SHAPE;
	else if(memcmp(magic, "LPHS", 4) == 0) return POP2_DATFORMAT_SHAPE_PALETTE;
	else if(memcmp(magic, "LRTS", 4) == 0) return POP2_DATFORMAT_TEXT;
	else if(memcmp(magic, "DNS\0", 4) == 0) return POP2_DATFORMAT_SOUND;
	else if(memcmp(magic, "SQES", 4) == 0) return POP2_DATFORMAT_SEQUENCE;
	else if(memcmp(magic, "4TXT", 4) == 0) return POP2_DATFORMAT_TEXT_ALT;
	else if(memcmp(magic, "\0\0\0\0", 4) == 0) return POP2_DATFORMAT_LEVEL;
	else return POP2_DATFORMAT_UNKNOWN;
}

bool Prince_OpenDAT(const char *path, int *entryCount)
{
	if(entryCount)
		*entryCount = 0;
	if(datContext.file != 0 || datContext.entryLists != 0)
	{
		StatusUpdate("Warning: Failed to load %s because we already have active pointers in datContext", path);
		return 0;
	}

	//Open DAT file
	fopen_s(&datContext.file, path, "rb");
	if(datContext.file == 0)
	{
		StatusUpdate("Warning: Failed to load %s", path);
		return 0;
	}

	//Read header
	datHeader_s header;
	fread(&header, sizeof(datHeader_s), 1, datContext.file);
	fseek(datContext.file, header.footerOffset, SEEK_SET);

	//Read footer and entry list
	datFooter_s footer;
	fread(&footer, sizeof(datFooter_s), 1, datContext.file);
	datContext.entryListCount = 1;
	datContext.totalFileCount = footer.entryCount;
	datContext.entryLists = new entryList_s[1];
	datContext.entryLists[0].entryCount = footer.entryCount;
	datContext.entryLists[0].type = POP1_DATFORMAT_BIN;

	//We store entry list in the same format as POP2, but POP1 has a slightly different format, so we read in the POP1 format and then convert it to the POP2 format
	datFooterEntry_s *intermediateList  = new datFooterEntry_s[footer.entryCount];
	fread(intermediateList, sizeof(datFooterEntry_s), footer.entryCount, datContext.file);
	datContext.entryLists[0].entries = new datFooterEntryV2_s[footer.entryCount];
	for(unsigned short i = 0; i < footer.entryCount; i++)
	{
		datContext.entryLists[0].entries[i].id = intermediateList[i].id;
		datContext.entryLists[0].entries[i].offset = intermediateList[i].offset;
		datContext.entryLists[0].entries[i].size = intermediateList[i].size;
		memcpy(datContext.entryLists[0].entries[i].flags, "\0\0\0", 3);
	}
	delete[]intermediateList;

	//DAT has been succcesfully loaded, but let's do a quick error check to verify the entry list looks okay
	for(int i = 0; i < datContext.entryLists[0].entryCount; i++)
	{
		if(datContext.entryLists[0].entries[i].offset + datContext.entryLists[0].entries[i].size + 1 > header.footerOffset) //We add one byte to compensate for checksum byte that exists for each file entry within a DAT
		{
			StatusUpdate("Warning: Entry %u offset or size is invalid in DAT %s", i, path);
			Prince_CloseDAT();
			return 0;
		}
	}

	//Finish
	if(entryCount)
		*entryCount = datContext.totalFileCount;
	return 1;
}

bool Prince_OpenDATv2(const char *path, int *entryCount)
{
	if(entryCount)
		*entryCount = 0;
	if(datContext.file != 0 || datContext.entryLists != 0)
	{
		StatusUpdate("Warning: Failed to load %s because we already have active pointers in datContext", path);
		return 0;
	}

	//Open DAT file
	fopen_s(&datContext.file, path, "rb");
	if(datContext.file == 0)
	{
		StatusUpdate("Warning: Failed to load %s", path);
		return 0;
	}

	//Read header
	datHeader_s header;
	fread(&header, sizeof(datHeader_s), 1, datContext.file);
	fseek(datContext.file, header.footerOffset, SEEK_SET);

	//Read master index
	datMasterIndex_s masterIndex;
	fread(&masterIndex, sizeof(datMasterIndex_s), 1, datContext.file);

	//Read in footer headers
	datFooterHeader_s *footerHeaders = new datFooterHeader_s[masterIndex.footerCount]; //This gets freed later in this function
	fread(footerHeaders, sizeof(datFooterHeader_s), masterIndex.footerCount, datContext.file);

	//Read in every entry list
	datContext.entryLists = new entryList_s[masterIndex.footerCount];
	datContext.entryListCount = masterIndex.footerCount;
	datContext.totalFileCount = 0;
	for(int i = 0; i < masterIndex.footerCount; i++)
	{
		datContext.entryLists[i].type = DefineTypeBasedOnMagic(footerHeaders[i].magic);
		fseek(datContext.file, header.footerOffset + footerHeaders[i].footerOffset, SEEK_SET);

		//Read footer
		datFooter_s footer;
		fread(&footer, sizeof(datFooter_s), 1, datContext.file);

		//Update entry counts
		datContext.entryLists[i].entryCount = footer.entryCount;
		datContext.totalFileCount += footer.entryCount;
		
		//Read entry list
		datContext.entryLists[i].entries = new datFooterEntryV2_s[footer.entryCount];
		fread(datContext.entryLists[i].entries, sizeof(datFooterEntryV2_s), footer.entryCount, datContext.file);
	}
	delete[]footerHeaders;

	//DAT has been succcesfully loaded, but let's do a quick error check to verify the entry lists look okay
	for(int j = 0; j < datContext.entryListCount; j++)
	{
		for(int i = 0; i < datContext.entryLists[j].entryCount; i++)
		{
			if(datContext.entryLists[j].entries[i].offset + datContext.entryLists[j].entries[i].size + 1 > header.footerOffset) //We add one byte to compensate for checksum byte that exists for each file entry within a DAT
			{
				StatusUpdate("Warning: Entry %u/%u offset or size is invalid in DAT %s", j, i, path);
				Prince_CloseDAT();
				return 0;
			}
		}
	}

	//Finish
	if(entryCount)
		*entryCount = datContext.totalFileCount;
	return 1;
}

bool Prince_CloseDAT() //This can be used to close v1 and v2 DAT files
{
	if(datContext.file == 0 || datContext.entryLists == 0)
	{
		StatusUpdate("Warning: Failed to close DAT file because of missing pointers in datContext");
		return 0;
	}
	fclose(datContext.file);
	for(int i = 0; i < datContext.entryListCount; i++)
		delete[]datContext.entryLists[i].entries;
	datContext.file = 0;
	delete[]datContext.entryLists;
	datContext.entryLists = 0;
	return 1;
}

bool Prince_LoadEntryFromDAT(unsigned char **data, unsigned int *size, int loadEntryIdx, int loadEntryId, unsigned short *entryId)
{
	if(entryId)
		*entryId = 0;
	if(loadEntryIdx == -1 && loadEntryId == -1)
	{
		StatusUpdate("Warning: Both loadEntryIdx and loadEntryId can't be -1 during F_Prince_LoadEntryFromDAT()");
		return 0;
	}
	if(datContext.file == 0 || datContext.entryLists == 0)
	{
		StatusUpdate("Warning: Failed to load DAT entry because of missing pointers in datContext");
		return 0;
	}

	//Find entry index if we're finding entry based on id rather than index
	if(loadEntryIdx == -1)
	{
		for(int j = 0; j < datContext.entryLists[0].entryCount; j++)
		{
			if(datContext.entryLists[0].entries[j].id == loadEntryId)
			{
				loadEntryIdx = j;
				break;
			}
		}
		if(loadEntryIdx == -1)
		{
			StatusUpdate("Warning: Could not find entry with id %u in DAT during F_Prince_LoadEntryFromDAT()", loadEntryId);
			return 0;
		}
	}

	if(loadEntryIdx >= (int) datContext.totalFileCount)
	{
		StatusUpdate("Warning: Tried to load entry %u from DAT file but there are only %u entries.", loadEntryIdx, datContext.totalFileCount);
		return 0;
	}
	*size = datContext.entryLists[0].entries[loadEntryIdx].size;
	*data = new unsigned char [*size];
	unsigned char checksumByte;
	fseek(datContext.file, datContext.entryLists[0].entries[loadEntryIdx].offset, SEEK_SET);
	fread(&checksumByte, 1, 1, datContext.file);
	fread(*data, *size, 1, datContext.file);
	if(entryId)
		*entryId = datContext.entryLists[0].entries[loadEntryIdx].id;
	return 1;
}

bool Prince_LoadEntryFromDATv2(unsigned char **data, unsigned int *size, int type, int loadEntryIdx, int loadEntryId, unsigned short *entryId, unsigned char *flags)
{
	if(entryId)
		*entryId = 0;
	if(flags)
		memcpy(flags, "\0\0\0", 3);
	if(loadEntryIdx == -1 && loadEntryId == -1)
	{
		StatusUpdate("Warning: Both loadEntryIdx and loadEntryId can't be -1 during F_Prince_LoadEntryFromDATv2()");
		return 0;
	}
	if(datContext.file == 0 || datContext.entryLists == 0)
	{
		StatusUpdate("Warning: Failed to load DAT entry because of missing pointers in datContext");
		return 0;
	}

	//Find type
	for(int i = 0; i < datContext.entryListCount; i++)
	{
		if(datContext.entryLists[i].type == type)
		{
			//Find entry index if we're finding entry based on id rather than index
			if(loadEntryIdx == -1)
			{
				for(int j = 0; j < datContext.entryLists[i].entryCount; j++)
				{
					if(datContext.entryLists[i].entries[j].id == loadEntryId)
					{
						loadEntryIdx = j;
						break;
					}
				}
				if(loadEntryIdx == -1)
				{
					StatusUpdate("Warning: Could not find entry with id %u in DAT during F_Prince_LoadEntryFromDATv2()", loadEntryId);
					return 0;
				}
			}

			if(loadEntryIdx >= (int) datContext.entryLists[i].entryCount)
			{
				StatusUpdate("Warning: Tried to load entry %u from DAT file but there are only %u entries of type %u.", loadEntryIdx, datContext.totalFileCount, type);
				return 0;
			}
			*size = datContext.entryLists[i].entries[loadEntryIdx].size;
			*data = new unsigned char [*size];

			unsigned char checksumByte;
			fseek(datContext.file, datContext.entryLists[i].entries[loadEntryIdx].offset, SEEK_SET);
			fread(&checksumByte, 1, 1, datContext.file);
			fread(*data, *size, 1, datContext.file);

			//Checksum verification
			unsigned char entrySumAlt = checksumByte;
			for(unsigned int k = 0; k < datContext.entryLists[i].entries[loadEntryIdx].size; k++)
			{
				entrySumAlt += (*data)[k];
			}
			//entrySumAlt should be 0xFF at this point

			if(entryId)
				*entryId = datContext.entryLists[i].entries[loadEntryIdx].id;
			if(flags)
				memcpy(flags, datContext.entryLists[i].entries[loadEntryIdx].flags, 3);
			return 1;
		}
	}
	StatusUpdate("Warning: Failed to find type %u during F_Prince_LoadEntryFromDATv2()");
	return 0;
}

int Prince_ReturnFileTypeCountFromDAT(int type) //This operation only works with a POP2 DAT file
{
	if(datContext.file == 0 || datContext.entryLists == 0)
	{
		StatusUpdate("Warning: Failed to load DAT entry because of missing pointers in datContext");
		return 0;
	}

	//Count entries matching count and return it
	int count = 0;
	for(int i = 0; i < datContext.entryListCount; i++)
	{
		if(datContext.entryLists[i].type == type)
			count += datContext.entryLists[i].entryCount;
	}
	return count;
}