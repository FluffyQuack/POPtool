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

#include <direct.h>
#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "misc.h"
#include "lodepng.h"

#define TAB 0x09
#define SPACE ' '
#define MAKEDIR_MAXSIZE 2000

char r_str[FILESTRINGMAX]; //String used by ReadValue
float r_float; //Value used by ReadValue
int r_int; //Value used by ReadValue
bool r_bool; //Value used by ReadValue_Bool

int LastSlash(char *path)
{
	int i = 0, slash = 0;
	while (1)
	{
		if (path[i] == 0)
			break;
		if ((path[i] == '/' || path[i] == '\\') && path[i + 1] != 0)
			slash = i;
		i++;
	}
	return slash;
}

int LastDot(char *path)
{
	int i = 0, dot = 0;
	while(1)
	{
		if(path[i] == 0)
			break;
		if(path[i] == '.' && path[i + 1] != 0)
			dot = i;
		i++;
	}
	return dot;
}

int FirstDot(char *path) //This finds the first dot after the last slash (to ensure we don't find the dot as part of a directory rather than file)
{
	int i = 0, dot = 0, slash = 0;
	while(1)
	{
		if(path[i] == '\\' || path[i] == '/')
		{
			slash = i;
			dot = 0;
		}
		if(path[i] == 0)
			break;
		if(path[i] == '.' && path[i + 1] != 0 && i > slash && dot == 0)
			dot = i;
		i++;
	}
	return dot;
}

bool ReadFile(const char *fileName, unsigned char **data, unsigned int *dataSize)
{
	//Open file
	FILE *file;
	fopen_s(&file, fileName, "rb");
	if(!file)
		return 0;
	fpos_t fpos = 0;
	_fseeki64(file, 0, SEEK_END);
	fgetpos(file, &fpos);
	_fseeki64(file, 0, SEEK_SET);
	*dataSize = (unsigned int) fpos;
	*data = new unsigned char[*dataSize];
	fread(*data, *dataSize, 1, file);
	fclose(file);
	return 1;
}

unsigned char CharToHex(const char *bytes, int offset)
{
	unsigned char value = 0;
	unsigned char finValue = 0;

	for(int i = 0; i < 2; i++)
	{
		char c = bytes[i + offset];
		if( c >= '0' && c <= '9' )
			value = c - '0';
        else if( c >= 'A' && c <= 'F' )
            value = 10 + (c - 'A');
        else if( c >= 'a' && c <= 'f' )
            value = 10 + (c - 'a');

		if(i == 0 && value > 0)
			value *= 16;

		finValue += value;
	}
	return finValue;
}

void CharByteToData(char *num, unsigned char *c, int size, bool littleEndian) //Note, num size has to be twice the size of "size" otherwise this function fails
{
	int j = 0;
	int i;
	if(littleEndian)
		i = 0;
	else
		i = size - 1;
	while(1)
	{
		c[i] = CharToHex(num, j);
		j += 2;
		if(littleEndian)
		{
			if(i == size - 1)
				break;
			i++;
		}
		else
		{
			if(i == 0)
				break;
			i--;
		}
	}
}

bool IsNumber(char *str)
{
	int i = 0;
	while (str[i] != 0)
	{
		if (!(str[i] >= '0' && str[i] <= '9'))
		{
			return 0;
		}
		i++;
	}
	return 1;
}

void RemoveLineBreaks(char *str)
{
	int i = 0;
	while (str[i] != 0)
	{
		if (str[i] == '\r' || str[i] == '\n')
		{
			str[i] = 0;
			return;
		}
		i++;
	}
	return;
}

int SymbolCountInString(char *str, char symbol)
{
	int count = 0, i = 0;
	while (str[i] != 0)
	{
		if (str[i] == symbol)
			count++;
		i++;
	}
	return count;
}

//Separate a string into 2 based on the first occurence of a symbol (it does not keep the separator in either string). Returns true if it found separator
bool SeparateString(char *str, char *str2, char seperator)
{
	bool copy = 0;
	int i = 0, j = 0;
	while(str[i] != 0)
	{
		if(copy)
			str2[j] = str[i], j++;
		if(str[i] == seperator)
			copy = 1, str[i] = 0;
		i++;
	}
	str2[j] = 0;
	return copy;
}

unsigned long long StringToULongLong(char *string)
{
	unsigned long long number = 0, num = 1;
	int length = (int) strlen(string);
	for(int i = length - 1; i >= 0; i--)
	{
		number += (string[i] - '0') * num;
		num *= 10;
	}
	return number;
}

char *ReturnStringPos(char *find, char *in) //Find position of "find" string within "in" string. Does a lower case comparison.
{
	int findPos = 0, inPos = 0, returnPos = 0;
	while (in[inPos] != 0)
	{
		if (tolower(in[inPos]) == tolower(find[findPos]))
		{
			if (findPos == 0)
				returnPos = inPos;
			findPos++;
			if (find[findPos] == 0)
				return &in[returnPos];
		}
		else
		{
			findPos = 0;
			if (returnPos > 0)
			{
				inPos = returnPos + 1;
				returnPos = 0;
			}
		}
		inPos++;
	}
	return 0;
}

//If the final characters are spaces, they are removed. If final characters are not space(s), nothing happens.
void RemoveSpacesAtEnd(char *str)
{
	int l,i=0,lastS=0;
	char c=str[i];
	while(c!=0)
	{
		if(c==SPACE)
			lastS=i;
		i++;
		c=str[i];
	}
	l=i;
	if(lastS==l-1)
	{
		for(i=l-1;i>0;i--)
		{
			if(str[i]==SPACE)
				str[i]=0;
			else
				break;
		}
	}
}

//Removes all spaces and tabs at the start of a string
void RemoveSpacesAtStart(char *str)
{
	if(str[0]!=SPACE && str[0]!=TAB) //If there's no space or tab at the beginning, we don't need to fix string
		return;

	int i=0,j=0;
	bool copyRest=0;
	while(str[i]!=0)
	{
		if(!copyRest)
		{
			if(str[i]==SPACE || str[i]==TAB)
			{
				i++;
				continue;
			}
			else
				copyRest=1;
		}
		if(copyRest)
		{
			str[j]=str[i];
			i++;
			j++;
		}
	}
	str[j]=0;
}

#define MAXSTATUSUPDATETEXTSIZE 260
void StatusUpdate(const char *text, ...)
{
	char str[MAXSTATUSUPDATETEXTSIZE];
	va_list	argumentPtr = NULL;
	va_start(argumentPtr, text);
	vsnprintf_s(str, MAXSTATUSUPDATETEXTSIZE, _TRUNCATE, text, argumentPtr);
	va_end(argumentPtr);
	printf("%s\n", str);
}

bool SaveImageAsPNG(char *path, unsigned char *imgData, unsigned int width, unsigned int height, unsigned char channels) //This takes raw RGB or RGBA image data as input
{
	MakeDirectory_PathEndsWithFile(path);
	FILE *file;
	fopen_s(&file, path, "wb");
	if(!file)
	{
		StatusUpdate("Warning: Failed to open %s for writing during SaveImageAsPNG()", path);
		return 0;
	}
	unsigned char *pngData = 0;
	size_t pngDataSize;
	unsigned int error;
	if(channels == 3)
		error = lodepng_encode24(&pngData, &pngDataSize, imgData, width, height);
	else if(channels == 4)
		error = lodepng_encode32(&pngData, &pngDataSize, imgData, width, height);
	if(error)
	{
		StatusUpdate("Lodepng error %u: %s\n", error, lodepng_error_text(error));
		fclose(file);
		return 0;
	}
	fwrite(pngData, pngDataSize, 1, file);
	free(pngData);
	fclose(file);
	StatusUpdate("Wrote %s", path);
	return 1;
}

//Make directory. This version handles a path with a file, so it'll not add a directory with file name. (TODO: This should be obsolete! Replace this with below function and confirm code still works, then remove this function)
void MakeDirectory_PathEndsWithFile(char *fullpath, int pos)
{
	char path[MAKEDIR_MAXSIZE];
	memset(path, 0, MAKEDIR_MAXSIZE);

	if(pos != 0)
		memcpy(path, fullpath, pos);

	while(1)
	{
		if(fullpath[pos] == 0)
			break;
		path[pos] = fullpath[pos];
		pos++;
		if(fullpath[pos] == '\\' || fullpath[pos] == '/')
			_mkdir(path);
	}
}

//Converts raw data (integers for examples) to show as hex via string (swapEndian should be 1 for big endian)
void DataToHex(char *to, char *from, int size, bool swapEndian)
{
	char const hextable[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

	int i, j=0;
	if(swapEndian)
		j=(size*2)-2;
	for(i=0;i<size;i++)
	{
		to[j]=hextable[ from[i] >> 4 & 0xf];
		to[j+1]=hextable[ from[i] & 0xf];
		if(swapEndian) j-=2;
		else j+=2;
	}
}

//This replaces the first found specific symbol in a string with null terminator, making the string end there
void ReplaceSymbolWithNullInString(char *string, char symbol)
{
	int i = 0;
	char c = string[i];
	while(c != 0)
	{
		if(c == symbol)
		{
			string[i] = 0;
			return;
		}
		i++;
		c = string[i];
	}
}

/*
- This is similar to functions below, but this is written to scan all tokens in a line for a string ala "TokenName=Value" and then return the value as the data type that "type" is defined as
- This is made to be used by script command, but it is slower than normal token reading, so it's recommended this is used rarely or that it's only used with compiled to binary
- If RV_STRING is used as target type, then result will be stored in r_str
- Note that this uses ReadValue with RV_STRING target, so this will always overwrite r_str no matter the target type
*/
bool ReturnTokenWithName(char *line, char *tokenName, int type, void *valuePtr, int curTokenTarget)
{
	//TODO: Should we make this go through Script_ReturnValue() so it can attain values stored in variables and pointers?

	char str2[FILESTRINGMAX] = {0};
	while(1)
	{
		if(!ReadValue(line, curTokenTarget, RV_STRING))
			return 0;
		if(SeparateString(r_str, str2, '=')) //Separates token into two. r_str will become token name, and str2 will become token value
		{
			if(_stricmp(r_str, tokenName) == 0)
			{
				if(type == RV_INT && valuePtr != 0)
					*((int *) valuePtr) = atoi(str2);
				else if(type == RV_FLOAT && valuePtr != 0)
					*((float *) valuePtr) = (float) atof(str2);
				else if(type == RV_INT && valuePtr != 0) //Bool is true if token value is above 0
					*((bool *) valuePtr) = atoi(str2) > 0;
				else if(type == RV_STRING) //If RV_STRING, then we store the result in r_str
					strcpy_s(r_str, FILESTRINGMAX, str2);
				return 1;
			}
		}
		curTokenTarget++;
	}
}

int ReturnTokenCount(char *str)
{
	int pos = 0, token = 0, endOfToken = 0;
	bool prevEnder = 1, quotationMark = 0;
	while(1)
	{
		bool doubleQuotation = 0; //If there's double quotation marks after finding one quotation mark, we'll treat it as a single normal quotation mark rather than a seperator
		if(quotationMark && str[pos] == '"' && str[pos + 1] != 0 && str[pos + 1] == '"' && str[pos + 2] != 0) 
			doubleQuotation = 1;
		else if(str[pos] == '"')
			quotationMark = !quotationMark;
		if(str[pos] == 0 || str[pos] == '#' || str[pos] == '\r' || str[pos] == '\n' || str[pos] == '\t' || (str[pos] == ' ' && !quotationMark) || (str[pos] == '"' && doubleQuotation == 0))
		{
			if(str[pos] == 0 || str[pos] == '#')
				break;
			prevEnder = 1;
		}
		else
		{
			if(prevEnder)
				token++;
			prevEnder = 0;
		}
		if(doubleQuotation)
			pos += 2;
		else
			pos++;
	}
	return token;
}

int ReturnTokenPos(char *str, int target, bool *quotationMarkPtr)
{
	//Find a specific "token" (tokens are separated by spaces, tabs, or single quotation marks)
	int pos = 0, token = 0, endOfToken = 0;
	bool prevEnder = 1, quotationMark = 0;
	while(1)
	{
		bool doubleQuotation = 0; //If there's double quotation marks after finding one quotation mark, we'll treat it as a single normal quotation mark rather than a seperator
		if(quotationMark && str[pos] == '"' && str[pos + 1] != 0 && str[pos + 1] == '"' && str[pos + 2] != 0) 
			doubleQuotation = 1;
		else if(str[pos] == '"')
			quotationMark = !quotationMark;
		if(str[pos] == 0 || str[pos] == '#' || str[pos] == '\r' || str[pos] == '\n' || str[pos] == '\t' || (str[pos] == ' ' && !quotationMark) || (str[pos] == '"' && doubleQuotation == 0)) //TODO: Should we remove the check for '#' since lines should be pre-processed to have that replaced with 0 anyway?
		{
			if(str[pos] == 0 || str[pos] == '#')
				break;
			prevEnder = 1;
		}
		else
		{
			if(prevEnder)
			{
				token++;
				if(token == target)
				{
					if(quotationMarkPtr)
						*quotationMarkPtr = quotationMark;
					return pos;
				}
			}
			prevEnder = 0;
		}
		if(doubleQuotation)
			pos += 2;
		else
			pos++;
	}
	return -1;
}

bool ReadValue(char *str, int target, int type, unsigned int maxLength)
{
	if(target == 0)
	{
		StatusUpdate("Warning: Target is 0 during ReadValue()");
		return 0;
	}

	if(type == RV_STRING)
	{
		if(maxLength > FILESTRINGMAX)
			maxLength = FILESTRINGMAX;

		if(str == r_str)
			StatusUpdate("Warning: ReadValue() called using r_str as str while searching for string!");
	}
	
	bool quotationMark = 0;
	int curPos2 = 0, curPos = ReturnTokenPos(str, target, &quotationMark);

	if(curPos != -1)
	{
		char str2[FILESTRINGMAX];
		while(1)
		{
			if(quotationMark && str[curPos] == '"' && str[curPos + 1] != 0 && str[curPos + 1] == '"') //If there's double quotation marks, we'll copy it over as a single quotation mark to string and not see it as an ender
				curPos++;
			else if(str[curPos] == 0 || str[curPos] == '#' || str[curPos] == '\r' || str[curPos] == '\n' || str[curPos] == '\t' || (str[curPos] == ' ' && !quotationMark) || str[curPos] == '"') //Space, tab, null, linebreak, or "" detected (space only seen as an ender if we're not reading a ""-style string)
			{
				str2[curPos2] = 0;

				//Make sure there's at least one digit in this string if we're outputting a float or int
				if(type == RV_FLOAT || type == RV_INT)
				{
					bool found = 0;
					for(int i = 0; ; i++)
					{
						if(str2[i] == 0 || str2[i] == '#' || str2[i] == '\r' || str2[i] == '\n')
							break;
						if(str2[i] >= '0' && str2[i] <= '9')
						{
							found = 1;
							break;
						}
					}
					if(!found)
					{
						if(type == RV_FLOAT)
							r_float = 0.0f;
						if(type == RV_INT)
							r_int = 0;
						return 0;
					}
				}

				if(type == RV_INT) //Output an int
				{
					r_int = atoi(str2);
					return 1;
				}
				else if(type == RV_STRING) //Output a string
				{
					if(curPos2 >= (int) maxLength)
					{
						StatusUpdate("Warning: String length limit %i exceeded for %s during ReadValue()", maxLength, str2);
						break;
					}
					memcpy(r_str, str2, curPos2 + 1);
					return 1;
				}
				else if(type == RV_FLOAT) //Output a float
				{
					r_float = (float) atof(str2);
					return 1;
				}
				else if(type == RV_BOOL) //Output a bool (using int as base)
				{
					if(atoi(str2) > 0)
						r_bool = 1;
					else
						r_bool = 0;
					return 1;
				}
			}

			str2[curPos2] = str[curPos];
			curPos++;
			curPos2++;
			if(curPos2 >= FILESTRINGMAX)
			{
				StatusUpdate("Warning: String size too long during ReadValue()");
				break;
			}
		}
	}

	if(type == RV_INT) r_int = 0;
	else if(type == RV_FLOAT) r_float = 0.0f;
	else if(type == RV_STRING) r_str[0] = 0;
	return 0;
}

bool ReadValue_Int(char *str, int target, int *var, unsigned int maxLength)
{
	if(!ReadValue(str, target, RV_INT, maxLength))
		return 0;
	*var = r_int;
	return 1;
}

bool ReadValue_Float(char *str, int target, float *var, unsigned int maxLength)
{
	if(!ReadValue(str, target, RV_FLOAT, maxLength))
		return 0;
	*var = r_float;
	return 1;
}

bool ReadValue_String(char *str, int target, char *str2, unsigned int maxLength)
{
	if(!ReadValue(str, target, RV_STRING, maxLength))
		return 0;
	strcpy_s(str2, maxLength, r_str);
	return 1;
}

bool ReadValue_Bool(char *str, int target, bool *var, unsigned int maxLength)
{
	if(!ReadValue(str, target, RV_BOOL, maxLength))
		return 0;
	*var = r_bool;
	return 1;
}

//Removes arbitrary symbol at the start of a string
void RemoveSymbolAtStart(char *str, char symbol)
{
	if(str[0] != symbol) //If symbol isn't at the beginning, we don't need to fix string
		return;

	int i = 0, j = 0;
	bool copyRest = 0;
	while(str[i] != 0)
	{
		if(!copyRest)
		{
			if(str[i] == symbol)
			{
				i++;
				continue;
			}
			else
				copyRest = 1;
		}
		if(copyRest)
		{
			str[j] = str[i];
			i++;
			j++;
		}
	}
	str[j] = 0;
}

//Removes arbitrary symbol at end of a string
void RemoveSymbolAtEnd(char *str, char symbol)
{
	int lastPosWithNonSymbol = -1, pos = 0, nullPos = 0;
	while(1)
	{
		if(str[pos] == 0)
		{
			nullPos = pos;
			break;
		}
		if(str[pos] != symbol)
			lastPosWithNonSymbol = pos;
		pos++;
	}

	if(lastPosWithNonSymbol == -1) //If this is -1, that means we never found any non-symbol characters
	{
		str[0] = 0;
		return;
	}

	if(lastPosWithNonSymbol + 1 < nullPos)
	{
		str[lastPosWithNonSymbol + 1] = 0;
		return;
	}
}