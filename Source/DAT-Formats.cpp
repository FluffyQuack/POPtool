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
#include "Vars.h"
#include "DAT.h"
#include "DAT-Formats.h"
#include "Misc.h"

//TODO: We should make it possible to specify offset for palette when calling Prince_ConvertPaletteToGeneric() - This would make it possible to access different parts of the guards palette from POP1 assets

#pragma pack(push, 1)
struct imgHeader_s
{
	unsigned short height;
	unsigned short width;

	//First 8 bits are always null for POP1 images (it's 1 for POP2 images that contain up to 256 colours). Next 4 bits are "resolution" and last 4 bits are compression type.
	unsigned char info[2];
};

struct palette_s //POP1
{
	unsigned char unknown[4];
	unsigned char vgaPal[48]; //16 RGB colours (6-bits per colour)
	unsigned char cgaPatterns[16];
	unsigned char egaPatterns[32];
};

struct paletteV2_s //POP2 (shape palette)
{
	unsigned char unknown[4];
	unsigned char unknown2[3];
	unsigned char vgaPal[48]; //16 RGB colours (6-bits per colour)
};

struct pop2_frame_type_exactSize {
	short image;
	short sword;
	char dx;
	char dy;
	unsigned char flags;
};
#pragma pack(pop)

void Prince_ConvertPaletteToGeneric(princeGenericPalette_s *genericPal, unsigned char *sourcePal, unsigned int sourcePalSize, int sourcePalType)
{
	memset(genericPal, 0, sizeof(princeGenericPalette_s));
	if(sourcePalType == POP1_DATFORMAT_PAL)
	{
		if(sourcePalSize != sizeof(palette_s))
		{
			StatusUpdate("Warning: Input palette size doesn't match size of POP1 palette.");
			return;
		}
		for(int colourNum = 0, k = 0; k < 48; k += 3, colourNum++)
		{
			genericPal->colours[colourNum].r = ((palette_s *) sourcePal)->vgaPal[k + 0] << 2;
			genericPal->colours[colourNum].g = ((palette_s *) sourcePal)->vgaPal[k + 1] << 2;
			genericPal->colours[colourNum].b = ((palette_s *) sourcePal)->vgaPal[k + 2] << 2;
		}
	}
	else if(sourcePalType == POP2_DATFORMAT_SHAPE_PALETTE)
	{
		for(int colourNum = 0, k = 0; k < 48; k += 3, colourNum++)
		{
			genericPal->colours[colourNum].r = ((paletteV2_s *) sourcePal)->vgaPal[k + 0] << 2;
			genericPal->colours[colourNum].g = ((paletteV2_s *) sourcePal)->vgaPal[k + 1] << 2;
			genericPal->colours[colourNum].b = ((paletteV2_s *) sourcePal)->vgaPal[k + 2] << 2;
		}
	}
	else if(sourcePalType == POP2_DATFORMAT_SVGA_PALETTE || sourcePalType == POP2_DATFORMAT_TGA_PALETTE || sourcePalType == POP1_DATFORMAT_MULTIPAL)
	{
		if(sourcePalSize / 3 > PRINCEMAXPALSIZE)
		{
			StatusUpdate("Warning: Input POP2 SVGA palette size is too big.");
			return;
		}
		for(unsigned int colourNum = 0, k = 0; k < sourcePalSize; k += 3, colourNum++)
		{
			genericPal->colours[colourNum].r = sourcePal[k + 0] << 2;
			genericPal->colours[colourNum].g = sourcePal[k + 1] << 2;
			genericPal->colours[colourNum].b = sourcePal[k + 2] << 2;
		}
	}
}

int Prince_GuessDATFormat(unsigned char *data, unsigned int dataSize, bool isPalLoaded) //This is only done for POP1 assets
{
	int format = -1;
	bool possiblyPAL = 0, possiblyIMG = 0;

	//Check if it's a palette
	if(dataSize == sizeof(palette_s))
	{
		//Check if every part of the VGA palette is 6 bits in size
		palette_s *pal = (palette_s *) data;
		possiblyPAL = 1;
		bool invalid = 0;
		for(int i = 0; i < 48; i++)
		{
			if(pal->vgaPal[i] >= 1 << 6)
			{
				possiblyPAL = 0;
				break;
			}
		}
	}

	//Check if it's an image
	if(isPalLoaded && dataSize > sizeof(imgHeader_s))
	{
		imgHeader_s *imgHeader = (imgHeader_s *) data;
		possiblyIMG = 1;
		if(imgHeader->height > 2048 || imgHeader->width > 2048 //Arbitrary limit
			|| imgHeader->height == 0 || imgHeader->width == 0
			|| imgHeader->info[0] != 0) //First 8 bits of info are always supposed to be 0 for POP1 images
			possiblyIMG = 0;
	}

	if(possiblyIMG && possiblyPAL)
	{
		StatusUpdate("Warning: Potential format confusion during GuessFormat()");
	}

	if(possiblyIMG)
		return POP1_DATFORMAT_IMG;
	if(possiblyPAL)
		return POP1_DATFORMAT_PAL;
	return POP1_DATFORMAT_BIN;
}

//Based on SDL-PoP code
void decompress_rle_lr(byte* destination,const byte* source,int dest_length) {
	const byte* src_pos = source;
	byte* dest_pos = destination;
	short rem_length = dest_length;
	while (rem_length) {
		signed char count = *src_pos;
		src_pos++;
		if (count >= 0) { // copy
			++count;
			do {
				*dest_pos = *src_pos;
				dest_pos++;
				src_pos++;
				--rem_length;
				--count;
			} while (count && rem_length);
		} else { // repeat
			byte al = *src_pos;
			src_pos++;
			count = -count;
			do {
				*dest_pos = al;
				dest_pos++;
				--rem_length;
				--count;
			} while (count && rem_length);
		}
	}
}

//Based on SDL-PoP code
void decompress_rle_ud(byte* destination,const byte* source,int dest_length,int width,int height) {
	short rem_height = height;
	const byte* src_pos = source;
	byte* dest_pos = destination;
	short rem_length = dest_length;
	--dest_length;
	--width;
	while (rem_length) {
		signed char count = *src_pos;
		src_pos++;
		if (count >= 0) { // copy
			++count;
			do {
				*dest_pos = *src_pos;
				dest_pos++;
				src_pos++;
				dest_pos += width;
				--rem_height;
				if (rem_height == 0) {
					dest_pos -= dest_length;
					rem_height = height;
				}
				--rem_length;
				--count;
			} while (count && rem_length);
		} else { // repeat
			byte al = *src_pos;
			src_pos++;
			count = -count;
			do {
				*dest_pos = al;
				dest_pos++;
				dest_pos += width;
				--rem_height;
				if (rem_height == 0) {
					dest_pos -= dest_length;
					rem_height = height;
				}
				--rem_length;
				--count;
			} while (count && rem_length);
		}
	}
}

//Based on SDL-PoP code
byte* decompress_lzg_lr(byte* dest,const byte* source,int dest_length) {
	byte* window = (byte*) malloc(0x400);
	if (window == NULL) return NULL;
	memset(window, 0, 0x400);
	byte* window_pos = window + 0x400 - 0x42; // bx
	short remaining = dest_length; // cx
	byte* window_end = window + 0x400; // dx
	const byte* source_pos = source;
	byte* dest_pos = dest;
	unsigned short mask = 0;
	do {
		mask >>= 1;
		if ((mask & 0xFF00) == 0) {
			mask = *source_pos | 0xFF00;
			source_pos++;
		}
		if (mask & 1) {
			*window_pos = *dest_pos = *source_pos;
			window_pos++;
			dest_pos++;
			source_pos++;
			if (window_pos >= window_end) window_pos = window;
			--remaining;
		} else {
			unsigned short copy_info = *source_pos;
			source_pos++;
			copy_info = (copy_info << 8) | *source_pos;
			source_pos++;
			byte* copy_source = window + (copy_info & 0x3FF);
			byte copy_length = (copy_info >> 10) + 3;
			do {
				*window_pos = *dest_pos = *copy_source;
				window_pos++;
				dest_pos++;
				copy_source++;
				if (copy_source >= window_end) copy_source = window;
				if (window_pos >= window_end) window_pos = window;
				--remaining;
				--copy_length;
			} while (remaining && copy_length);
		}
	} while (remaining);
//	end:
	free(window);
	return dest;
}

//Based on SDL-PoP code
byte* decompress_lzg_ud(byte* dest,const byte* source,int dest_length,int stride,int height) {
	byte* window = (byte*) malloc(0x400);
	if (window == NULL) return NULL;
	memset(window, 0, 0x400);
	byte* window_pos = window + 0x400 - 0x42; // bx
	short remaining = height; // cx
	byte* window_end = window + 0x400; // dx
	const byte* source_pos = source;
	byte* dest_pos = dest;
	unsigned short mask = 0;
	short dest_end = dest_length - 1;
	do {
		mask >>= 1;
		if ((mask & 0xFF00) == 0) {
			mask = *source_pos | 0xFF00;
			source_pos++;
		}
		if (mask & 1) {
			*window_pos = *dest_pos = *source_pos;
			window_pos++;
			source_pos++;
			dest_pos += stride;
			--remaining;
			if (remaining == 0) {
				dest_pos -= dest_end;
				remaining = height;
			}
			if (window_pos >= window_end) window_pos = window;
			--dest_length;
		} else {
			unsigned short copy_info = *source_pos;
			source_pos++;
			copy_info = (copy_info << 8) | *source_pos;
			source_pos++;
			byte* copy_source = window + (copy_info & 0x3FF);
			byte copy_length = (copy_info >> 10) + 3;
			do {
				*window_pos = *dest_pos = *copy_source;
				window_pos++;
				copy_source++;
				dest_pos += stride;
				--remaining;
				if (remaining == 0) {
					dest_pos -= dest_end;
					remaining = height;
				}
				if (copy_source >= window_end) copy_source = window;
				if (window_pos >= window_end) window_pos = window;
				--dest_length;
				--copy_length;
			} while (dest_length && copy_length);
		}
	} while (dest_length);
//	end:
	free(window);
	return dest;
}

//Based on SDL-PoP code
void decompr_img(byte* dest,const unsigned char *sourceData ,int decomp_size,int compressMethod, int depth, int height, int stride) {
	switch (compressMethod) {
		case 0: // RAW left-to-right
			memcpy(dest, sourceData, decomp_size);
		break;
		case 1: // RLE left-to-right
			decompress_rle_lr(dest, sourceData, decomp_size);
		break;
		case 2: // RLE up-to-down
			decompress_rle_ud(dest, sourceData, decomp_size, stride, height);
		break;
		case 3: // LZG left-to-right
			decompress_lzg_lr(dest, sourceData, decomp_size);
		break;
		case 4: // LZG up-to-down
			decompress_lzg_ud(dest, sourceData, decomp_size, stride, height);
		break;
	}
}

//Based on SDL-PoP code
unsigned char *conv_to_8bpp(unsigned char *in_data, int width, int height, int stride, int depth) {
	unsigned char *out_data = new unsigned char[width * height];
	int pixels_per_byte = 8 / depth;
	int mask = (1 << depth) - 1;
	for (int y = 0; y < height; ++y) {
		unsigned char* in_pos = in_data + y*stride;
		unsigned char* out_pos = out_data + y*width;
		for (int x_pixel = 0, x_byte = 0; x_byte < stride; ++x_byte) {
			unsigned char v = *in_pos;
			int shift = 8;
			for (int pixel_in_byte = 0; pixel_in_byte < pixels_per_byte && x_pixel < width; ++pixel_in_byte, ++x_pixel) {
				shift -= depth;
				*out_pos = (v >> shift) & mask;
				++out_pos;
			}
			++in_pos;
		}
	}
	return out_data;
}

//Based on PR code
/* modulus to be used in the 10 bits of the algorithm */
#define LZG_WINDOW_SIZE    0x400 /* =1024=1<<10 */

/* LZG expansion algorithm sub function */
unsigned char popBit(unsigned char *byte)
{
	register unsigned char bit=(*byte)&1;
	(*byte)>>=1;
	return bit;
}

//Based on PR code
/* Expands LZ Groody algorithm. This is the core of PR */
int expandLzg(const unsigned char* input, int inputSize, unsigned char** output2, int *outputSize)
{

	int                    oCursor=0, iCursor=0;
	unsigned char          maskbyte=0;
	/* TODO: Don't reserve more than needed. */
	//unsigned char          output[65500];
	unsigned char *output = new unsigned char[65500];
	register int           loc;
	register unsigned char rep, k;

	if(*outputSize)
		*outputSize += LZG_WINDOW_SIZE;
	else
		*outputSize = 65500;

	/* initialize the first 1024 bytes of the window with zeros */
	for(oCursor=0;oCursor<LZG_WINDOW_SIZE;output[oCursor++]=0);

	/* main loop */
	while (iCursor<inputSize&&(oCursor)<(*outputSize)) {
		maskbyte=input[iCursor++];
		for (k=8;k&&(iCursor<inputSize)&&(oCursor)<(*outputSize);k--) { /* fix by David */
			if (popBit(&maskbyte)) {
				output[oCursor++]=input[iCursor++]; /* copy input to output */
			} else {
				/*
				 * loc:
				 *  10 bits for the slide iCursorition (S). Add 66 to this number,
				 *  substract the result to the current oCursor and take the last 10 bits.
				 * rep:
				 *  6 bits for the repetition number (R). Add 3 to this number.
				 */
				loc= 66 + ((input[iCursor] & 0x03 /*00000011*/) <<8) + input[iCursor+1];
				rep= 3  + ((input[iCursor] & 0xfc /*11111100*/) >>2);

				iCursor+=2; /* move the cursor 2 bytes ahead */

				loc=(oCursor-loc)&0x3ff; /* this is the real loc number (allways positive!) */
				if(loc==0) loc=0x400; /* by David */

				while (rep--) { /* repeat pattern in output */
					output[oCursor]=output[oCursor-loc];
					oCursor++;
				}
			}
		}
	}

	inputSize-=iCursor;
	/* ignore the first 1024 bytes */
	*outputSize=oCursor-LZG_WINDOW_SIZE;
	
	//*output2=(unsigned char*)malloc(*outputSize);
	*output2= new unsigned char[*outputSize];

	for(iCursor=LZG_WINDOW_SIZE;iCursor<oCursor;iCursor++)
		(*output2)[iCursor-LZG_WINDOW_SIZE]=output[iCursor];

	delete[]output;

	if(oCursor>=(*outputSize))
		return inputSize; /* TODO: check if this case never happens !!! */

	return (!maskbyte)-1;
	/*return rep?COMPRESS_RESULT_WARNING:COMPRESS_RESULT_SUCCESS;*/
}

//Based on PR code
/* Expands RLE algorithm */
bool expandRleC(const unsigned char *input, int inputSize, unsigned char *output, int *outputSize)
{
	register unsigned char rep = 0;
	int oCursor = 0;
	int iCursor = 0;

	/* main loop */
	while(iCursor < inputSize)
	{
		rep = (input[iCursor++]);
		if(rep&0x80) /* repeat */
		{ 
			rep&=(~0x80);
			rep++;
			while (rep--) (output)[oCursor++]=input[iCursor];
			iCursor++;
		}
		else /* jump */
		{
			rep++;
			while ((rep--)&&(iCursor<inputSize))
			{
				(output)[oCursor++]=input[iCursor++];
			}
		}
	}
	
	*outputSize=oCursor;
	return (rep==255)?1:0;
}

//Based on PR code
bool pop2decompress(const unsigned char* input, int inputSize, int verify, unsigned char** output,unsigned int* outputSize)
{
	unsigned char* tempOutput;
	unsigned char* lineI; /* chunk */
	unsigned char* lineO; /* chunk */
	int            lineSize;
	int            aux, remaining = inputSize;
	int            tempOutputSize;

	*output = new unsigned char[64000]; //320x200 = 64000
	lineO = *output;
	*outputSize = 0;

	while(remaining)
	{
		tempOutputSize = (unsigned short &) *input;
		input += 2; inputSize -= 2;
		remaining=expandLzg(input,inputSize,&tempOutput,&tempOutputSize);

		/* Second layer expand each rle line */
		lineI=tempOutput;
		do
		{
			aux = (unsigned short &) *lineI;
			lineI += 2;
			if (aux > tempOutputSize)
				return 0;
			expandRleC(lineI, aux, lineO, &lineSize);
			lineO += lineSize;
			*outputSize += lineSize;
			tempOutputSize -= aux;
			tempOutputSize -= 2;
			lineI += aux;
		} while(lineSize == verify && tempOutputSize > 0);
        {
           int oldinputSize = inputSize;
           inputSize = remaining;
		   input += (oldinputSize-inputSize);
        }
		delete[]tempOutput;
     }
	return 1;
}

bool Prince_ConvPOPImageData(unsigned char *srcImgData, unsigned int srcImgDataSize, princeGenericPalette_s *paletteData, unsigned char **destImgData, unsigned int *destImgDataSize, unsigned int *width, unsigned int *height, unsigned char *channels, bool flipY)
{
	//Check pointers
	if(destImgData == 0 || destImgDataSize == 0 || height == 0 || width == 0 || channels == 0 || paletteData == 0)
	{
		StatusUpdate("Warning: One incoming pointer is null during Prince_ConvPOPImageData()");
		return 0;
	}

	//Init outgoing values
	*destImgData = 0;
	*destImgDataSize = 0;
	*height = 0;
	*width = 0;
	*channels = 0;

	//Check if incoming image data exists
	if(srcImgData == 0 || paletteData == 0)
	{
		StatusUpdate("Warning: srcImgData or paletteData is null during Prince_ConvPOPImageData()");
		return 0;
	}

	//Check if this can't be valid image data
	imgHeader_s *header = (imgHeader_s *) srcImgData;
	if(srcImgDataSize <= sizeof(imgHeader_s) || header->height == 0 || header->width == 0 || header->height > 2048 || header->width > 2048)
	{
		StatusUpdate("Warning: srcImgData contains invalid POP image data");
		return 0;
	}

	//Read header, allocate outgoing image data buffer, and define outgoing values
	*height = header->height;
	*width = header->width;
	*channels = 4; //TODO: Should we check if there's any alpha in the image and change this to 3 if there's none?
	*destImgDataSize = *height * *width * *channels;
	*destImgData = new unsigned char[*destImgDataSize];

	//Decode image data into 8-bit palletized image data
	unsigned char *rawImgData = 0;
	unsigned int rawImgDataSize = *height * *width;
	{
		int depth = ((header->info[1] >> 4) & 7) + 1;
		int compressMethod = (header->info[1]) & 0x0F;
		int stride = (depth * *width + 7) / 8;
		if(header->info[0] == 1) //This is used for POP2 images that have up to 256 colours
		{
			pop2decompress(&srcImgData[sizeof(imgHeader_s)], srcImgDataSize - sizeof(imgHeader_s), header->width, &rawImgData, &rawImgDataSize);
		}
		else //This is used for all graphical assets in POP1 and most sprites in POP2
		{
			unsigned int intDataSize = *height * stride;
			unsigned char *intData = new unsigned char[intDataSize]; //Intermediate data
			decompr_img(intData, &srcImgData[sizeof(imgHeader_s)], *height * stride, compressMethod, depth, *height, stride);
			rawImgData = conv_to_8bpp(intData, *width, *height, stride, depth); //Convert to raw 8-bit palletized image data
			delete[]intData;
		}
	}

	//Convert to raw RGBA
	{
		int stride = *width * *channels;
		unsigned int destPos = flipY ? (*height - 1) * stride : 0;
		for(unsigned int srcPos = 0, stridePos = 0; srcPos < rawImgDataSize; destPos += 4, srcPos++, stridePos += 4)
		{
			if(flipY && stridePos == stride)
			{
				stridePos = 0;
				destPos -= stride * 2;
			}

			(*destImgData)[destPos + 0] = paletteData->colours[rawImgData[srcPos]].r;
			(*destImgData)[destPos + 1] = paletteData->colours[rawImgData[srcPos]].g;
			(*destImgData)[destPos + 2] = paletteData->colours[rawImgData[srcPos]].b;

			if(rawImgData[srcPos] == 0) //First palette entry is transparent entry
				(*destImgData)[destPos + 3] = 0;
			else
				(*destImgData)[destPos + 3] = 255;
		}
		delete[]rawImgData;
	}

	return 1;
}

static bool PathWithoutExt(const char *inPath, char *outPath)
{
	int lastDot = -1, pos = 0;
	while(inPath[pos] != 0)
	{
		if(inPath[pos] == '.')
			lastDot = pos;
		pos++;
	}
	if(lastDot == -1)
	{
		StatusUpdate("Warning: Could not find extension in path during PathWithoutExt()");
		return 0;
	}
	strcpy(outPath, inPath);
	outPath[lastDot] = 0;
	return 1;
}

bool Prince_ExtractDAT(const char *path, unsigned char *palData, unsigned int palSize, int palType)
{
	bool failed = 0;

	//Variant of path without extension
	char pathWithoutExt[MAX_PATH] = {0};
	PathWithoutExt(path, pathWithoutExt);

	int imageCount = 0;
	unsigned char *fileData = 0;
	unsigned int fileDataSize = 0;
	princeGenericPalette_s palette;
	bool palLoaded = 0;

	if(palData)
	{
		Prince_ConvertPaletteToGeneric(&palette, palData, palSize, palType);
		palLoaded = 1;
	}

	if(Prince_OpenDAT(path, &imageCount))
	{
		unsigned short id;
		for(int i = 0; i < imageCount; i++)
		{
			if(fileData)
			{
				delete[]fileData;
				fileData = 0;
			}
			if(!Prince_LoadEntryFromDAT(&fileData, &fileDataSize, i, -1, &id))
			{
				failed = 1;
				break;
			}

			int format = Prince_GuessDATFormat(fileData, fileDataSize, palLoaded);
			if(format == POP1_DATFORMAT_PAL && !palLoaded)
			{
				Prince_ConvertPaletteToGeneric(&palette, fileData, fileDataSize, POP1_DATFORMAT_PAL);
				palLoaded = 1;
			}
			else if(format == POP1_DATFORMAT_IMG)
			{
				unsigned char *newImgData = 0;
				unsigned int newImgDataSize = 0, width = 0, height = 0;
				unsigned char channels = 0;
				if(!Prince_ConvPOPImageData(fileData, fileDataSize, &palette, &newImgData, &newImgDataSize, &width, &height, &channels))
				{
					failed = 1;
					break;
				}

				//Convert to PNG
				char pngPath[MAX_PATH];
				sprintf_s(pngPath, MAX_PATH, "%s\\res%u.png", pathWithoutExt, id);
				SaveImageAsPNG(pngPath, newImgData, width, height, channels);
				delete[]newImgData;
			}

			if(format != POP1_DATFORMAT_IMG) //Write data to file (unless it's an image file, which we already would have converted and saved as PNG)
			{
				char binPath[MAX_PATH];
				if(format == POP1_DATFORMAT_PAL)
					sprintf_s(binPath, MAX_PATH, "%s\\res%u.pal", pathWithoutExt, id);
				else
					sprintf_s(binPath, MAX_PATH, "%s\\res%u.bin", pathWithoutExt, id);
				MakeDirectory_PathEndsWithFile(binPath);
				FILE *file;
				fopen_s(&file, binPath, "wb");
				if(!file)
				{
					StatusUpdate("Warning: Failed to open %s for writing.", binPath);
					failed = 1;
					break;
				}
				fwrite(fileData, fileDataSize, 1, file);
				fclose(file);
				StatusUpdate("Wrote %s", binPath);
			}
		}
		Prince_CloseDAT();
	}
	else
		failed = 1;

	//Finish
	if(fileData)
		delete[]fileData;
	return !failed;
}

static const char *PredefinedPOP2ScriptAnimName(int id)
{
	switch(id)
	{
	case 1: return "StartRun";
	case 2: return "Stand";
	case 3: return "StandingForwardJump";
	case 4: return "RunningJump";
	case 5: return "Turn";
	case 6: return "RunningTurn";
	case 7: return "StartFall1";
	case 8: return "JumpUpToLedge_NoX";
	case 9: return "Hang";
	case 10: return "ClimbUp";
	case 11: return "FallOntoTile";
	case 12: return "Falling";
	case 13: return "RunStop";
	case 14: return "JumpUpAndHitCeiling";
	case 15: return "GrabLedgeMidAir";
	case 16: return "JumpUpToLedge_NoTileBehind";
	case 17: return "LandingAfterShortFall";
	case 18: return "Falling_AfterForwardJump";
	case 19: return "StartFall0";
	case 20: return "HardFall1";
	case 21: return "FallAfterRunningJump";
	case 22: return "DeadAfterFall";
	case 23: return "ReleaseLedge";
	case 24: return "JumpUpToLedge";
	case 25: return "HangStraightAgainstWall";
	case 26: return "CrouchSlide";
	case 27: return "FallIntoQuicksand";
	case 28: return "JumpUpIntoAir";
	case 29: return "Step1";
	case 30: return "Step2";
	case 31: return "Step3";
	case 32: return "Step4";
	case 33: return "Step5";
	case 34: return "Step6";
	case 35: return "Step7";
	case 36: return "Step8";
	case 37: return "Step9";
	case 38: return "Step10";
	case 39: return "Step11";
	case 40: return "Step12";
	case 41: return "Step13";
	case 42: return "FullStep";
	case 43: return "StartRun0";
	case 44: return "TestFoot";
	case 45: return "FallBump";
	case 46: return "JumpIntoWall";
	case 47: return "Bump";
	case 48: return "Unknown";
	case 49: return "StandUpFromCrouch";
	case 50: return "Crouch";
	case 51: return "WallSpikeDeath_Left";
	case 52: return "GetHitByFallingTile";
	case 53: return "Unknown";
	case 54: return "WallSpikeDeath_Right";
	case 55: return "UnsheatheSword";
	case 56: return "ForwardWithSword";
	case 57: return "BackWithSword";
	case 58: return "SwordStrike1";
	case 59: return "ClimbUpIntoBoat";
	case 60: return "Guard_Turn";
	case 61: return "ParryAfterBeingParried";
	case 62: return "Parry1";
	case 63: return "LandEnGarde";
	case 64: return "BumpEngarde_Forward";
	case 65: return "BumpEngarde_Backward_MostLikely";
	case 66: return "StrikeAfterParry";
	case 67: return "Unknown";
	case 68: return "ClimbDown";
	case 69: return "BeingParried";
	case 70: return "ExitLevel";
	case 71: return "AligntoFloorAndDropDead";
	case 72: return "Unknown";
	case 73: return "ClimbUpFail";
	case 74: return "GetHurtSwordFighting";
	case 75: return "SwordStrike2";
	case 76: return "Unknown";
	case 77: return "GuardStanding";
	case 78: return "DrinkPotion";
	case 79: return "CrouchHop";
	case 80: return "GuardIdleFlip";
	case 81: return "FallBack";
	case 82: return "Guard_Falling";
	case 83: return "Guard_FallingDownToFollowPlayer";
	case 84: return "Guard_StartRun";
	case 85: return "Die1";
	case 86: return "GuardAdvance";
	case 87: return "ChasingSkeletonAttacksPlayer";
	case 88: return "SkeletonRising";
	case 89: return "Unknown";
	case 90: return "GuardEnteringFightingStance";
	case 91: return "PickupSword";
	case 92: return "Sheathe";
	case 93: return "FastSheathe";
	case 94: return "GetHurtSwordFightingFromBehind";
	case 95: return "FallAfterAdvancingWithSword";
	case 96: return "SwordStrikeLow";
	case 97: return "HitBySnake";
	case 98: return "CrawlDie";
	case 99: return "ReleaseLedgeTowardsTile";
	case 100: return "Guard_RunningJump";
	case 101: return "Guard_EndRun";
	case 102: return "GuardSkeleton_StartRun";
	case 103: return "Unknown";
	case 104: return "FlameSword_Retreat";
	case 105: return "FlameSword_LyingOnFloor";
	case 106: return "FlameSword_WakeUp";
	case 107: return "FlameSword_Die";
	case 108: return "FlameSword_Advance";
	case 109: return "FlameSword_Strike";
	case 110: return "GuardBird_Worship";
	case 111: return "ThrowingAwayBottle";
	case 112: return "RattleSkeletonRemains";
	case 113: return "Sliced";
	case 114: return "RunningJumpFallCloseToEdge";
	case 115: return "RunIntoLava";
	case 116: return "FallIntoLava";
	case 117: return "CrouchLoop";
	case 118: return "CrushedByDoor";
	case 119: return "Skeleton_LyingDead";
	case 120: return "Skeleton_Collapsing";
	case 121: return "CrawlIdle";
	case 122: return "CrawlForward";
	case 123: return "CrawlBackwards";
	case 124: return "CrawlToStand";
	case 125: return "StandToCrawl";
	case 126: return "Skeleton_HitBySpikes";
	case 127: return "TurnWhileFighting";
	case 128: return "SitOnMagicCarpet";
	case 129: return "SitOnMagicCarpet_DuringCutscene0";
	case 130: return "Head_Spin1";
	case 131: return "Head_Spin2";
	case 132: return "Head_Spin3";
	case 133: return "Head_Spin4";
	case 134: return "Head_Spin5";
	case 135: return "Head_Spin6";
	case 136: return "Head_Spin7";
	case 137: return "Head_Spin8";
	case 138: return "Head_Spin9";
	case 139: return "Head_AngryToIdle";
	case 140: return "Head_ScreamWhenSeeingPlayer";
	case 141: return "Head_AngryIdle";
	case 142: return "Head_AngryStartMoveForward";
	case 143: return "Head_AngryStartMoveUp";
	case 144: return "Head_AngryStartMoveDown";
	case 145: return "Head_CollideWithWall_Die";
	case 146: return "Head_HitByFallingTile";
	case 147: return "Head_BecomeAngry";
	case 150: return "Head_Attack1";
	case 151: return "Head_Attack2";
	case 152: return "Head_Attack3";
	case 153: return "Head_Hurt";
	case 154: return "Head_Die";
	case 155: return "Head_Idle";
	case 156: return "Head_AngryFlip";
	case 157: return "Head_CollideWithWall_Flip";
	case 158: return "Head_FloatIntoWall";
	case 159: return "Head_BecomeSad";
	case 160: return "Head_SulkingStill0";
	case 161: return "Head_SulkingStill1";
	case 162: return "Head_SulkingToScream";
	case 163: return "Head_SulkingToIdle";
	case 164: return "Head_CollideWithWall_Die_Variant";
	case 165: return "Head_AttackLoop_1";
	case 166: return "Head_MaybeReturnToIdleAfterSeeingPlayer";
	case 168: return "Snake_BumpIntoWall";
	case 169: return "Snake_HitByFallingTile";
	case 170: return "Snake_Advance";
	case 171: return "Snake_EnterHoleInGround";
	case 172: return "Snake_ExitHoleInGround";
	case 173: return "Snake_Attack2";
	case 174: return "Snake_Die";
	case 175: return "Snake_Attack1";
	case 176: return "Snake_RecoilBeforeAttacking";
	case 177: return "Snake_AbandonAttack";
	case 180: return "JinneeAppearing";
	case 184: return "Unknown";
	case 185: return "Guard_FallingToOffscreen";
	case 186: return "Guard_FallingAfterRunningJump";
	case 187: return "Guard_HittingGroundAfterRunningJumpFall";
	case 188: return "Guard_WallSpikeDeath_Left";
	case 189: return "Guard_WallSpikeDeath_Right";
	case 190: return "Guard_GetSliced";
	case 191: return "GuardBird_Dead1";
	case 192: return "GuardBird_Dead2";
	case 193: return "GuardBird_Dead3";
	case 194: return "GuardBird_Dead4";
	case 195: return "Guard_Dead";
	case 196: return "FakePrinceDisappears";
	case 197: return "FakePrinceLaughs";
	case 198: return "FallTurnOnBridge";
	case 199: return "SitOnMagicCarpet_DuringCutscene";
	case 200: return "StartRun";
	case 201: return "RunLoop1";
	case 202: return "RunLoop2";
	case 203: return "SwordStrike3";
	case 204: return "SwordStrike4";
	case 205: return "SwordStrike5";
	case 206: return "Parry2";
	case 207: return "Unknown";
	case 208: return "Guard_Run";
	case 209: return "Unknown";
	case 210: return "Hang";
	case 211: return "StepExtend";
	case 212: return "HardFall2_Or_GuardGoingPoof";
	case 213: return "Die2";
	case 214: return "StartFall2";
	case 215: return "EnterBoat";
	case 216: return "GuardSkeleton_RunLoop";
	case 217: return "Unknown";
	case 218: return "Head_AngryMovingForward";
	case 219: return "Head_AngryMovingUp";
	case 220: return "Head_AngryMovingDown";
	case 221: return "Head_AngryIdle";
	case 222: return "Head_Sulking";
	case 223: return "Snake_Idle";
	case 225: return "Unknown";
	case 226: return "Head_AttackLoop_2";
	case 227: return "EnGarde";
	case 228: return "StartSlowFall";
	case 229: return "SlowFall_BumpIntoWall";
	case 230: return "DieFromTouchingFlame";
	case 231: return "RiseFromDeath";
	case 232: return "Unknown";
	case 235: return "FinishingClimbUp";
	case 236: return "ClimbingUpAndLosingSword";
	case 237: return "PickUpAndBeShocked";
	case 238: return "GuardAppearingWithSmoke";
	case 239: return "GuardGoingPoofWithSmoke";
	case 240: return "RealFakePrinceDisappearing";
	case 241: return "FailToUseSword";
	case 242: return "UseSpell";
	case 243: return "FakePrinceDying";
	case 244: return "Unknown";
	}
	return 0;
}

bool Prince_ExtractDATv2(const char *path, unsigned char *palData, unsigned int palSize, int palType, int startId, int endId)
{
	bool success = 1;

	//Variant of path without extension
	char pathWithoutExt[MAX_PATH] = {0};
	PathWithoutExt(path, pathWithoutExt);

	int totalEntryCount = 0;
	unsigned char *fileData = 0;
	unsigned int fileDataSize = 0;
	princeGenericPalette_s palette;
	bool palLoaded = 0;

	if(palData)
	{
		Prince_ConvertPaletteToGeneric(&palette, palData, palSize, palType);
		palLoaded = 1;
	}

	if(Prince_OpenDATv2(path, &totalEntryCount))
	{
		FILE *sequenceOutput = 0;
		for(int type = POP2_DATFORMAT_UNKNOWN; type <= POP2_DATFORMAT_LEVEL; type++)
		{
			int entryCount = Prince_ReturnFileTypeCountFromDAT(type);
			for(int i = 0; i < entryCount; i++)
			{
				if(fileData)
				{
					delete[]fileData;
					fileData = 0;
				}
				unsigned short id;
				unsigned char flags[3];
				if(!Prince_LoadEntryFromDATv2(&fileData, &fileDataSize, type, i, -1, &id, flags))
				{
					success = 0;
					break;
				}

				if(((startId != -1 && id < startId) || (endId != -1 && id > endId)) //If startId and endId are defined then we skip assets that don't match those IDs
					&& type != POP2_DATFORMAT_CGA_PALETTE && type != POP2_DATFORMAT_SVGA_PALETTE && type != POP2_DATFORMAT_TGA_PALETTE && type != POP2_DATFORMAT_SHAPE_PALETTE) //However, we always allow loading of palletes
					continue;

				if(strstr(path, "TRANS.DAT") && id == 25381)
				{
					StatusUpdate("Warning: Skipping TRANS.DAT %u since it doesn't convert correctly.");
					continue;
				}

				const char *typeDir = 0;
				if(type == POP2_DATFORMAT_UNKNOWN) typeDir = "Unknown";
				else if(type == POP2_DATFORMAT_CUSTOM) typeDir = "Custom";
				else if(type == POP2_DATFORMAT_FONT) typeDir = "Fonts";
				else if(type == POP2_DATFORMAT_FRAME) typeDir = "Frames";
				else if(type == POP2_DATFORMAT_CGA_PALETTE) typeDir = "cgaPalette";
				else if(type == POP2_DATFORMAT_SVGA_PALETTE) typeDir = "svgaPalette";
				else if(type == POP2_DATFORMAT_TGA_PALETTE) typeDir = "tgaPalette";
				else if(type == POP2_DATFORMAT_PIECE) typeDir = "Pieces";
				else if(type == POP2_DATFORMAT_PSL) typeDir = "LSP";
				else if(type == POP2_DATFORMAT_SCREEN) typeDir = "Screens";
				else if(type == POP2_DATFORMAT_SHAPE) typeDir = "Shapes";
				else if(type == POP2_DATFORMAT_SHAPE_PALETTE) typeDir = "ShapePalettes";
				else if(type == POP2_DATFORMAT_TEXT) typeDir = "Text";
				else if(type == POP2_DATFORMAT_SOUND) typeDir = "Sounds";
				else if(type == POP2_DATFORMAT_SEQUENCE) typeDir = "Sequences";
				else if(type == POP2_DATFORMAT_TEXT_ALT) typeDir = "Text4";
				else if(type == POP2_DATFORMAT_LEVEL) typeDir = "Levels";
				else typeDir = "Invalid";
				char binPath[MAX_PATH];
				sprintf_s(binPath, MAX_PATH, "%s\\%s\\res%u-%u-%u-%u.bin", pathWithoutExt, typeDir, id, flags[0], flags[1], flags[2]);
				MakeDirectory_PathEndsWithFile(binPath);

				FILE *file;
				fopen_s(&file, binPath, "wb");
				if(!file)
				{
					StatusUpdate("Warning: Failed to open %s for writing.", binPath);
					success = 0;
					break;
				}
				fwrite(fileData, fileDataSize, 1, file);
				fclose(file);
				StatusUpdate("Wrote %s", binPath);

				if((type == POP2_DATFORMAT_SHAPE_PALETTE || type == POP2_DATFORMAT_SVGA_PALETTE || type == POP2_DATFORMAT_TGA_PALETTE) && !palLoaded) //Save palette so we can use it for image conversion
				{
					Prince_ConvertPaletteToGeneric(&palette, fileData, fileDataSize, type);
					palLoaded = 1;
				}
				else if(type == POP2_DATFORMAT_SHAPE && palLoaded && fileDataSize > sizeof(imgHeader_s) && ((imgHeader_s *) fileData)->height != 0 && ((imgHeader_s *) fileData)->width != 0 && ((imgHeader_s *) fileData)->height <= 2048 && ((imgHeader_s *) fileData)->width <= 2048)
				{
					unsigned char *newImgData = 0;
					unsigned int newImgDataSize = 0, width = 0, height = 0;
					unsigned char channels = 0;
					if(!Prince_ConvPOPImageData(fileData, fileDataSize, &palette, &newImgData, &newImgDataSize, &width, &height, &channels))
					{
						success = 0;
						break;
					}

					//Convert to PNG
					char pngPath[MAX_PATH];
					sprintf_s(pngPath, MAX_PATH, "%s\\%s\\res%u-%u-%u-%u.png", pathWithoutExt, typeDir, id, flags[0], flags[1], flags[2]);
					SaveImageAsPNG(pngPath, newImgData, width, height, channels);
					delete[]newImgData;
				}
				else if(type == POP2_DATFORMAT_SOUND && fileDataSize > 4 && memcmp(&fileData[1], "MThd", 4) == 0) //This is a MIDI file
				{
					sprintf_s(binPath, MAX_PATH, "%s\\%s\\res%u-%u-%u-%u.mid", pathWithoutExt, typeDir, id, flags[0], flags[1], flags[2]);
					MakeDirectory_PathEndsWithFile(binPath);

					file = 0;
					fopen_s(&file, binPath, "wb");
					if(!file)
					{
						StatusUpdate("Warning: Failed to open %s for writing.", binPath);
						success = 0;
						break;
					}
					fwrite(&fileData[1], fileDataSize - 1, 1, file);
					fclose(file);
					StatusUpdate("Wrote %s", binPath);
				}
				else if(type == POP2_DATFORMAT_SEQUENCE)
				{
					bool firstSeq = 0;
					if(!sequenceOutput)
					{
						firstSeq = 1;
						sprintf_s(binPath, MAX_PATH, "%s\\Sequences.txt", pathWithoutExt);
						MakeDirectory_PathEndsWithFile(binPath);
						fopen_s(&sequenceOutput, binPath, "wb");

						if(!sequenceOutput)
						{
							StatusUpdate("Warning: Failed to open %s for writing.", binPath);
							success = 0;
							break;
						}
					}
					
					//Parse sequence
					if(sequenceOutput)
					{
						if(!firstSeq) //Divider between animations
							fprintf(sequenceOutput, "\r\n\r\n");

						unsigned int pos = 0;
						const char *scriptName = PredefinedPOP2ScriptAnimName(id);
						if(scriptName)
							fprintf(sequenceOutput, "[POP2_%03i_%s]", id, scriptName);
						else
							fprintf(sequenceOutput, "[POP2_%03i]", id);
						while(pos + 1 < fileDataSize)
						{
							fprintf(sequenceOutput, "\r\n");
							short op = (short &) (fileData[pos]); pos += 2;							
							switch(op)
							{
								case -63:
								{
									fprintf(sequenceOutput, "UnknownOp%i", abs(op));
									break;
								}
								case -36:
								{
									fprintf(sequenceOutput, "UnknownOp%i", abs(op));
									break;
								}
								case -33:
								{
									fprintf(sequenceOutput, "UnknownOp%i", abs(op));
									break;
								}
								case -30:
								{
									fprintf(sequenceOutput, "UnknownOp%i", abs(op));
									break;
								}
								case -27:
								{
									fprintf(sequenceOutput, "UnknownOp%i", abs(op));
									break;
								}
								case -24: //seq_ffe8_set_palette
								{
									short val = (short &) (fileData[pos]); pos += 2;
									fprintf(sequenceOutput, "SetPalette %i", val);
									break;
								}
								case -23: //seq_ffe9
								{
									fprintf(sequenceOutput, "RepeatLastFrame");
									break;
								}
								case -22: //seq_ffea_random_branch
								{
									short val = (short &) (fileData[pos]); pos += 2;
									short val2 = (short &) (fileData[pos]); pos += 2;
									short val3 = (short &) (fileData[pos]); pos += 2;

									//Write first part of command
									fprintf(sequenceOutput, "RandomBranch %i", val);

									//Write two script names
									for(int i = 0; i < 2; i++)
									{
										int branchingId = i == 0 ? val2 : val3;
										const char *scriptName = PredefinedPOP2ScriptAnimName(branchingId);
										if(scriptName)
											fprintf(sequenceOutput, " POP2_%03i_%s", branchingId, scriptName);
										else
											fprintf(sequenceOutput, " POP2_%03i", branchingId);
									}
									break;
								}
								case -21: //seq_ffeb
								{
									short val = (short &) (fileData[pos]); pos += 2;
									fprintf(sequenceOutput, "SetSpecialState %i", val);
									break;
								}
								case -19: //seq_ffed_align_to_floor
								{
									fprintf(sequenceOutput, "AlignToFloor");
									break;
								}
								case -18: //seq_ffee
								{
									fprintf(sequenceOutput, "ResetSetAnim");
									break;
								}
								case -17: //seq_ffef_disappear
								{
									fprintf(sequenceOutput, "Disappear");
									break;
								}
								case -16: //seq_fff0_end_level
								{
									fprintf(sequenceOutput, "EndLevel");
									break;
								}
								case -15: //seq_fff1_sound
								{
									short snd = (short &) (fileData[pos]); pos += 2;
									fprintf(sequenceOutput, "PlaySoundPOP2 %i", snd);
									break;
								}
								case -14: //seq_fff2_getitem
								{
									short item = (short &) (fileData[pos]); pos += 2;
									fprintf(sequenceOutput, "GetItem %i", item);
									break;
								}
								case -13: //seq_fff3_knockdown
								{
									fprintf(sequenceOutput, "KnockDown");
									break;
								}
								case -12: //seq_fff4_knockup
								{
									fprintf(sequenceOutput, "KnockUp");
									break;
								}
								case -11: //seq_fff5
								{
									short val = (short &) (fileData[pos]); pos += 2;
									fprintf(sequenceOutput, "SetDeathType %i", val);
									break;
								}
								case -10: //seq_fff6_jump_if_slow
								{
									short animId = (short &) (fileData[pos]); pos += 2;
									const char *scriptName = PredefinedPOP2ScriptAnimName(animId);
									if(scriptName)
										fprintf(sequenceOutput, "Anim_IfFeather POP2_%03i_%s", animId, scriptName);
									else
										fprintf(sequenceOutput, "Anim_IfFeather POP2_%03i", animId);
									break;
								}
								case -9: //seq_fff7
								{
									short val = (short &) (fileData[pos]); pos += 2;
									short val2 = (short &) (fileData[pos]); pos += 2;
									fprintf(sequenceOutput, "AddMomentum %i %i", val, val2);
									break;
								}
								case -8: //seq_fff8_setfall
								{
									short speed = (short &) (fileData[pos]); pos += 2;
									short speed2 = (short &) (fileData[pos]); pos += 2;
									fprintf(sequenceOutput, "SetFall %i %i", speed, speed2);
									break;
								}
								case -7: //seq_fff9_action
								{
									short action = (short &) (fileData[pos]); pos += 2;
									if(action == 0) fprintf(sequenceOutput, "Action Stand");
									else if(action == 1) fprintf(sequenceOutput, "Action RunJump");
									else if(action == 2) fprintf(sequenceOutput, "Action HangClimb");
									else if(action == 3) fprintf(sequenceOutput, "Action InMidair");
									else if(action == 4) fprintf(sequenceOutput, "Action InFreefall");
									else if(action == 5) fprintf(sequenceOutput, "Action Bumped");
									else if(action == 6) fprintf(sequenceOutput, "Action HangStraight");
									else if(action == 7) fprintf(sequenceOutput, "Action Turn");
									else if(action == 8) fprintf(sequenceOutput, "Action Jinnee");
									else if(action == 9) fprintf(sequenceOutput, "Action FallingIntoForeground");
									else if(action == 99) fprintf(sequenceOutput, "Action Hurt");
									else fprintf(sequenceOutput, "Action %i", action);
									break;
								}
								case -6: //seq_fffa_dy
								{
									short val1 = (short &) (fileData[pos]); pos += 2;
									fprintf(sequenceOutput, "MoveY %i", val1);
									break;
								}
								case -5: //seq_fffb_dx
								{
									short val1 = (short &) (fileData[pos]); pos += 2;
									fprintf(sequenceOutput, "MoveX %i", val1);
									break;
								}
								case -4: //seq_fffc_down
								{
									fprintf(sequenceOutput, "MoveDown");
									break;
								}
								case -3: //seq_fffd_up
								{
									fprintf(sequenceOutput, "MoveUp");
									break;
								}
								case -2: //seq_fffe_flip
								{
									fprintf(sequenceOutput, "Flip");
									break;
								}
								case -1: //seq_ffff_jump
								{
									short animId = (short &) (fileData[pos]); pos += 2;
									const char *scriptName = PredefinedPOP2ScriptAnimName(animId);
									if(scriptName)
										fprintf(sequenceOutput, "Anim POP2_%03i_%s", animId, scriptName);
									else
										fprintf(sequenceOutput, "Anim POP2_%03i", animId);
									break;
								}
								default: //Animation frame
								{
									//fprintf(sequenceOutput, "ShowFrame %i; Wait", op);
									fprintf(sequenceOutput, "ShowFrame %i", op);
									break;
								}
							}
						}
					}
				}
			}
		}
		if(sequenceOutput)
		{
			fclose(sequenceOutput);
			StatusUpdate("Wrote sequence script file.");
		}
		Prince_CloseDAT();
	}
	else
		success = 0;

	//Finish
	if(fileData)
		delete[]fileData;
	return success;
}

bool Prince_ReadPOP2FrameArrayData(char *path)
{
	//Verify size of struct
	{
		int structSize = sizeof(pop2_frame_type_exactSize);
		if(structSize != 7)
		{
			StatusUpdate("Struct size is bonk.");
			return 0;
		}
	}

	//Open file
	unsigned char *data;
	unsigned int dataSize;
	if(!ReadFile(path, &data, &dataSize))
	{
		StatusUpdate("Could not open %s for reading.", path);
		return 0;
	}

	//Confirm that the file size is divisible by 7
	{
		int remainder = dataSize % sizeof(pop2_frame_type_exactSize);
		if(remainder != 0)
		{
			StatusUpdate("Remainder is bonk.");
			delete[]data;
			return 0;
		}
	}

	//Variant of path without extension
	char pathWithoutExt[MAX_PATH] = {0};
	PathWithoutExt(path, pathWithoutExt);

	//Output binary data in text form
	char outPath[MAX_PATH];
	sprintf_s(outPath, MAX_PATH, "%s\\FrameArray.txt", pathWithoutExt);
	MakeDirectory_PathEndsWithFile(outPath);
	FILE *file;
	fopen_s(&file, outPath, "wb");
	if(file)
	{
		int frameCount = dataSize / sizeof(pop2_frame_type_exactSize);
		fprintf(file, "pop_frame_type frame_table_kid[%i] = {\r\n", frameCount);
		pop2_frame_type_exactSize *array = (pop2_frame_type_exactSize *) data;
		for(int i = 0; i < frameCount; i++)
		{
			//Flags minus weight
			char hex[3] = {0,0,0};
			unsigned char hexFrom = array[i].flags - (array[i].flags & 0x1F);
			DataToHex(hex, (char *) &hexFrom, 1);

			//Sword flags
			char hex2[5] = {0,0,0,0,0};
			unsigned short hexFrom2 = array[i].sword - (array[i].sword & 0x3F);
			DataToHex(hex2, (char *) &hexFrom2, 2, 1);

			fprintf(file, "{ %4i, 0x%s|%2i, %3i, %3i, 0x%s|%2i},\r\n", array[i].image, hex2, array[i].sword & 0x3F, array[i].dx, array[i].dy, hex, array[i].flags & 0x1F);
		}
		fprintf(file, "};");
		fclose(file);
		StatusUpdate("Wrote %s", outPath);
		return 1;
	}
	return 0;
}