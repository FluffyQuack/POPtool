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

enum
{
	//POP1 DAT formats (we have to guess these based on file data)
	POP1_DATFORMAT_BIN, //Generic/unknown
	POP1_DATFORMAT_IMG, //Image data
	POP1_DATFORMAT_PAL, //Palette
	POP1_DATFORMAT_MULTIPAL, //File containing multiple VGA palettes

	//POP2 DAT formats (these are explicitly defined within DAT files)
	POP2_DATFORMAT_UNKNOWN,
	POP2_DATFORMAT_CGA_PALETTE, //"CLAP"
	POP2_DATFORMAT_SVGA_PALETTE, //"SLAP"
	POP2_DATFORMAT_TGA_PALETTE, //"TLAP"
	POP2_DATFORMAT_SHAPE_PALETTE, //"LPHS"
	POP2_DATFORMAT_CUSTOM, //"TSUC"
	POP2_DATFORMAT_FONT, //"TNOF"
	POP2_DATFORMAT_FRAME, //"MARF"
	POP2_DATFORMAT_PIECE, //"CEIP"
	POP2_DATFORMAT_PSL, //"LSP\0" (unknown type)
	POP2_DATFORMAT_SCREEN, //"RCS\0" (graphics for a room as one image)
	POP2_DATFORMAT_SHAPE, //"PAHS" (normal graphics)
	POP2_DATFORMAT_TEXT, //"LRTS"
	POP2_DATFORMAT_SOUND, //"DNS\0"
	POP2_DATFORMAT_SEQUENCE, //"SQES"
	POP2_DATFORMAT_TEXT_ALT, //"4TXT"
	POP2_DATFORMAT_LEVEL, //"\0\0\0\0"
};

void Prince_ConvertPaletteToGeneric(princeGenericPalette_s *genericPal, unsigned char *sourcePal, unsigned int sourcePalSize, int sourcePalType);
int Prince_GuessDATFormat(unsigned char *data, unsigned int dataSize, bool isPalLoaded);
bool Prince_ConvPOPImageData(unsigned char *srcImgData, unsigned int srcImgDataSize, princeGenericPalette_s *paletteData, unsigned char **rawImgData, unsigned int *rawImgDataSize, unsigned int *width, unsigned int *height, unsigned char *channels, bool flipY = 0);
bool Prince_ExtractDAT(const char *path, unsigned char *palData = 0, unsigned int palSize = 0, int palType = 0);
bool Prince_ExtractDATv2(const char *path, unsigned char *palData = 0, unsigned int palSize = 0, int palType = 0, int startId = -1, int endId = -1);
bool Prince_ReadPOP2FrameArrayData(char *path);