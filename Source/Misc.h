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

//Maximum size of string read by ReadValue
#define FILESTRINGMAX 2000

#define MAXPATH 260

//Read value enums
enum
{
	RV_INT,
	RV_STRING,
	RV_FLOAT,
	RV_BOOL
};

extern char r_str[FILESTRINGMAX]; //String used by ReadValue
extern float r_float; //Value used by ReadValue
extern int r_int; //Value used by ReadValue
extern bool r_bool; //Value used by ReadValue_Bool

int LastSlash(char *path);
int LastDot(char *path);
int FirstDot(char *path);
bool ReadFile(const char *fileName, unsigned char **data, unsigned int *dataSize);
unsigned char CharToHex(const char *bytes, int offset = 0);
void CharByteToData(char *num, unsigned char *c, int size, bool littleEndian); //Note, num size has to be twice the size of "size" otherwise this function fails
bool IsNumber(char *str);
void RemoveLineBreaks(char *str);
int SymbolCountInString(char *str, char symbol);
bool SeparateString(char *str, char *str2, char seperator);
unsigned long long StringToULongLong(char *string);
char *ReturnStringPos(char *find, char *in); //Find position of "find" string within "in" string. Does a lower case comparison.
void RemoveSpacesAtEnd(char *str);
void RemoveSpacesAtStart(char *str);
void StatusUpdate(const char *text, ...);
bool SaveImageAsPNG(char *path, unsigned char *imgData, unsigned int width, unsigned int height, unsigned char channels);
void MakeDirectory_PathEndsWithFile(char *fullpath, int pos = 0);
void DataToHex(char *to, char *from, int size, bool swapEndian = 0);
void ReplaceSymbolWithNullInString(char *string, char symbol);
bool ReturnTokenWithName(char *line, char *tokenName, int type = RV_INT, void *valuePtr = 0, int curTokenTarget = 1);
int ReturnTokenCount(char *str);
int ReturnTokenPos(char *str, int target, bool *quotationMarkPtr);
bool ReadValue(char* str, int target, int type = 0, unsigned int maxLength = FILESTRINGMAX);
//bool ReadValue_Alt(char *str, int target, unsigned int maxLength = FILESTRINGMAX);
bool ReadValue_Int(char *str, int target, int *var, unsigned int maxLength = FILESTRINGMAX);
bool ReadValue_Float(char *str, int target, float *var, unsigned int maxLength = FILESTRINGMAX);
bool ReadValue_String(char *str, int target, char *str2, unsigned int maxLength = FILESTRINGMAX);
bool ReadValue_Bool(char *str, int target, bool *var, unsigned int maxLength = FILESTRINGMAX);
void RemoveSymbolAtStart(char *str, char symbol);
void RemoveSymbolAtEnd(char *str, char symbol);
