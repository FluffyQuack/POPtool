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

#define MAXLINELENGTH 1000

struct labelPos_s
{
	int animIndex;
	int byteCodePos;
	char *label;
};

//Copies one line of text. Returns 1 if it's at the end of the data (end of line is signified by a semi-colon or linebreak)
static bool CopyLineFromAnimationScript(char *dest, const char *src, unsigned int *pos, unsigned int maxSize, unsigned int maxLineLength)
{
	bool foundRLineBreak = 0, foundNLineBreak = 0, endOfLine = 0;
	unsigned int i = 0;
	while(1)
	{
		if(i + *pos >= maxSize)
		{
			endOfLine = 1;
			break;
		}
		if(i + 1 >= maxLineLength)
			break;
		if(foundNLineBreak && foundRLineBreak)
			break;
		if(foundRLineBreak && src[i + *pos] != '\n')
			break;
		if(foundNLineBreak && src[i + *pos] != '\r')
			break;
		dest[i] = src[i + *pos];
		if(src[i + *pos] == ';')
		{
			dest[i] = 0;
			foundNLineBreak = 1;
			foundRLineBreak = 1;
		}
		else if(src[i + *pos] == '\n' || src[i + *pos] == '\r')
		{
			if(src[i + *pos] == '\n')
				foundNLineBreak = 1;
			if(src[i + *pos] == '\r')
				foundRLineBreak = 1;
			dest[i] = 0;
		}
		i++;
	}
	dest[i] = 0;
	*pos += i;
	return endOfLine;
}

static void AddNewLabelStructToArray(char *label, int animIndex, int byteCodePos, labelPos_s **labelArray, int *labelArraySize)
{
	//New array
	labelPos_s *newArray = new labelPos_s[*labelArraySize + 1];
	
	//Copy existing parts of the array if it already exists
	if(*labelArray)
	{
		memcpy(newArray, *labelArray, sizeof(labelPos_s) * *labelArraySize);
		delete[]*labelArray;
	}

	//Populate new entry
	newArray[*labelArraySize].label = new char[strlen(label) + 1];
	strcpy(newArray[*labelArraySize].label, label);
	newArray[*labelArraySize].animIndex = animIndex;
	newArray[*labelArraySize].byteCodePos = byteCodePos;

	//Increase array size integer
	*labelArraySize += 1;

	//Set new label array pointer
	*labelArray = newArray;
}

static int GetNumberFromScriptName(const char *scriptName)
{
	char shortName[100];
	{
		int pos = 0, toPos = 0;
		bool firstNonZero = 0;
		while(scriptName[pos] >= '0' && scriptName[pos] <= '9')
		{
			if(scriptName[pos] != '0' || firstNonZero)
			{
				firstNonZero = 1;
				shortName[toPos] = scriptName[pos];
				toPos++;
			}
			pos++;
		}
		shortName[toPos] = 0;
	}
	return atoi(shortName);
}


bool Prince_RepackDATv2(char *path)
{
	//Right now this only supports sequence.dat
	if(_stricmp(path, "sequence.dat") != 0)
	{
		StatusUpdate("Warning: Repacking only works with sequence.dat");
		return 0;
	}

	//Read sequences.txt
	char *scriptData = 0;
	unsigned int scriptDataSize = 0;
	if(!ReadFile("sequence\\sequences.txt", (unsigned char **) &scriptData, &scriptDataSize))
	{
		StatusUpdate("Warning: Could not load sequence\\sequences.txt for reading.");
		return 0;
	}

	//Process sequences.txt
	labelPos_s *labelsFound = 0, *jumpsToFix = 0; //Some bytecode needs to be fixed after processing all animations, and we keep track of it with these dynamic arrays
	int labelsFoundNum = 0, jumpsToFixNum = 0;
	int animIndex = -1;
	int p_animationCount = 0;
	princeAnim_s *p_animations = 0;
	bool finalLine = 0;
	unsigned int pos = 0, byteCodeBufferPos = 0;
	char line[MAXLINELENGTH];
	bool currentlyReadingAnimScript = 0;
	unsigned char *byteCodeBuffer = new unsigned char[scriptDataSize]; //We use filesize as maximum buffer size for the byde code which should be big enough.
	datFooterEntryV2_s *footerEntries = 0;
	FILE *file = 0;
	unsigned int animationDataSize = 0;
	unsigned int currentPos = 0;
	while(!finalLine) //Read file line-by-line
	{
		finalLine = CopyLineFromAnimationScript(line, scriptData, &pos, scriptDataSize, MAXLINELENGTH - 1); //Read next line
		ReplaceSymbolWithNullInString(line, '#'); //Replace '#' with null as that means comment
		RemoveSpacesAtStart(line);

		if(currentlyReadingAnimScript && line[0] != '[' && line[0] != 0) //This is a line of script code we need to convert into byte code
		{
			ReadValue(line, 1, RV_STRING);
			if(_stricmp(r_str, "Action") == 0) //This is followed by an argument specifying what type of 
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -7; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
				if(!ReadValue(line, 2, RV_INT))
				{
					StatusUpdate("Warning: Invalid second argument %s for Action script command", r_str);
					delete[]byteCodeBuffer;
					goto fail;
				}
				(short &) byteCodeBuffer[byteCodeBufferPos] = (short) r_int; byteCodeBufferPos += sizeof(short);
			}
			else if(_stricmp(r_str, "Anim") == 0 || _stricmp(r_str, "Anim_IfFeather") == 0) //This is followed by an argument specifying script function (optionally, it can have a second component which specifies where in a script function to jump to)
			{
				if(_stricmp(r_str, "Anim") == 0)
					(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -1;
				else
					(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -10; //Jump to animation start (same as above, but only happens if player has feather effect on)
				byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
				if(!ReadValue(line, 2, RV_STRING))
				{
					StatusUpdate("Warning: Invalid second argument %s for Anim script command", r_str);
					delete[]byteCodeBuffer;
					goto fail;
				}
				AddNewLabelStructToArray(r_str, animIndex, byteCodeBufferPos, &jumpsToFix, &jumpsToFixNum);
				byteCodeBufferPos += sizeof(short); //One short: animation id
			}
			else if(_stricmp(r_str, "ShowFrame") == 0)
			{
				//(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = PRINCE_AP_SHOWFRAME; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
				if(!ReadValue(line, 2, RV_INT))
				{
					StatusUpdate("Warning: Invalid second argument %s for ShowFrame script command", r_str);
					delete[]byteCodeBuffer;
					goto fail;
				}
				(short &) byteCodeBuffer[byteCodeBufferPos] = (short) r_int; byteCodeBufferPos += sizeof(short);
			}
			else if(_stricmp(r_str, "PlaySound") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -15; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
				if(!ReadValue(line, 2, RV_INT))
				{
					StatusUpdate("Warning: Invalid second argument %s for PlaySound script command", r_str);
					delete[]byteCodeBuffer;
					goto fail;
				}
				(short &) byteCodeBuffer[byteCodeBufferPos] = (short) r_int; byteCodeBufferPos += sizeof(short); //TODO
			}
			else if(_stricmp(r_str, "MoveX") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -5; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
				if(!ReadValue(line, 2, RV_INT))
				{
					StatusUpdate("Warning: Invalid second argument %s for MoveX script command", r_str);
					delete[]byteCodeBuffer;
					goto fail;
				}
				(short &) byteCodeBuffer[byteCodeBufferPos] = (short) r_int; byteCodeBufferPos += sizeof(short);
			}
			else if(_stricmp(r_str, "MoveY") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -6; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
				if(!ReadValue(line, 2, RV_INT))
				{
					StatusUpdate("Warning: Invalid second argument %s for MoveY script command", r_str);
					delete[]byteCodeBuffer;
					goto fail;
				}
				(short &) byteCodeBuffer[byteCodeBufferPos] = (short) r_int; byteCodeBufferPos += sizeof(short);
			}
			else if(_stricmp(r_str, "SetFall") == 0) //Defines falling?
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -8; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
				if(!ReadValue(line, 2, RV_INT))
				{
					StatusUpdate("Warning: Invalid second argument %s for SetFall script command", r_str);
					delete[]byteCodeBuffer;
					goto fail;
				}
				(short &) byteCodeBuffer[byteCodeBufferPos] = (short) r_int; byteCodeBufferPos += sizeof(short);
				if(!ReadValue(line, 3, RV_INT))
				{
					StatusUpdate("Warning: Invalid third argument %s for SetFall script command", r_str);
					delete[]byteCodeBuffer;
					goto fail;
				}
				(short &) byteCodeBuffer[byteCodeBufferPos] = (short) r_int; byteCodeBufferPos += sizeof(short);
			}
			else if(_stricmp(r_str, "Flip") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -2; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
			}
			else if(_stricmp(r_str, "KnockDown") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -13; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
			}
			else if(_stricmp(r_str, "KnockUp") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -12; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
			}
			else if(_stricmp(r_str, "MoveDown") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -4; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
			}
			else if(_stricmp(r_str, "MoveUp") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -3; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
			}
			else if(_stricmp(r_str, "GetItem") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -14; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
				if(!ReadValue(line, 2, RV_INT))
				{
					StatusUpdate("Warning: Invalid second argument %s for GetItem script command", r_str);
					delete[]byteCodeBuffer;
					goto fail;
				}
				(short &) byteCodeBuffer[byteCodeBufferPos] = (short) r_int; byteCodeBufferPos += sizeof(short);
			}
			else if(_stricmp(r_str, "EndLevel") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -16; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
			}
			else if(_stricmp(r_str, "UnknownOp23") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -23; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
			}
			else if(_stricmp(r_str, "UnknownOp21") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -21; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
				if(!ReadValue(line, 2, RV_INT))
				{
					StatusUpdate("Warning: Invalid second argument %s for Unknown21 script command", r_str);
					delete[]byteCodeBuffer;
					goto fail;
				}
				(short &) byteCodeBuffer[byteCodeBufferPos] = (short) r_int; byteCodeBufferPos += sizeof(short);
			}
			else if(_stricmp(r_str, "UnknownOp18") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -18; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
			}
			else if(_stricmp(r_str, "UnknownOp11") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -11; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
				if(!ReadValue(line, 2, RV_INT))
				{
					StatusUpdate("Warning: Invalid second argument %s for Unknown11 script command", r_str);
					delete[]byteCodeBuffer;
					goto fail;
				}
				(short &) byteCodeBuffer[byteCodeBufferPos] = (short) r_int; byteCodeBufferPos += sizeof(short);
			}
			else if(_stricmp(r_str, "UnknownOp9") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -9; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
				if(!ReadValue(line, 2, RV_INT))
				{
					StatusUpdate("Warning: Invalid second argument %s for Unknown9 script command", r_str);
					delete[]byteCodeBuffer;
					goto fail;
				}
				(short &) byteCodeBuffer[byteCodeBufferPos] = (short) r_int; byteCodeBufferPos += sizeof(short);
				if(!ReadValue(line, 3, RV_INT))
				{
					StatusUpdate("Warning: Invalid third argument %s for Unknown9 script command", r_str);
					delete[]byteCodeBuffer;
					goto fail;
				}
				(short &) byteCodeBuffer[byteCodeBufferPos] = (short) r_int; byteCodeBufferPos += sizeof(short);
			}
			else if(_stricmp(r_str, "Disappear") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -17; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
			}
			else if(_stricmp(r_str, "AlignToFloor") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -19; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
			}
			else if(_stricmp(r_str, "SetPalette") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -24; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
				if(!ReadValue(line, 2, RV_INT))
				{
					StatusUpdate("Warning: Invalid second argument %s for SetPalette script command", r_str);
					delete[]byteCodeBuffer;
					goto fail;
				}
				(short &) byteCodeBuffer[byteCodeBufferPos] = (short) r_int; byteCodeBufferPos += sizeof(short);
			}
			else if(_stricmp(r_str, "RandomBranch") == 0)
			{
				(PRINCE_AP_OPSIZE &) byteCodeBuffer[byteCodeBufferPos] = -22; byteCodeBufferPos += sizeof(PRINCE_AP_OPSIZE);
				if(!ReadValue(line, 2, RV_INT))
				{
					StatusUpdate("Warning: Invalid second argument %s for RandomBranch script command", r_str);
					delete[]byteCodeBuffer;
					goto fail;
				}
				(short &) byteCodeBuffer[byteCodeBufferPos] = (short) r_int; byteCodeBufferPos += sizeof(short);
			}
			else
			{
				StatusUpdate("Warning: Unidentified script command %s", r_str);
				delete[]byteCodeBuffer;
				goto fail;
			}
		}

		if(currentlyReadingAnimScript && (line[0] == '[' || finalLine)) //End of file or new script animation function, so finalize processing of current animation script function
		{
			p_animations[animIndex].byteCodeSize = byteCodeBufferPos;
			if(byteCodeBufferPos)
			{
				p_animations[animIndex].byteCode = new unsigned char[byteCodeBufferPos];
				memcpy(p_animations[animIndex].byteCode, byteCodeBuffer, byteCodeBufferPos);
				byteCodeBufferPos = 0;
			}
		}

		if(line[0] == '[') //Script name, so increase animation array by one
		{
			currentlyReadingAnimScript = 1;
			animIndex++;
				
			//Increase size of animation array
			princeAnim_s *newAnimArray = new princeAnim_s[animIndex + 1];
			if(p_animations) //Copy over and then delete animations we already created
			{
				memcpy(newAnimArray, p_animations, sizeof(princeAnim_s) * p_animationCount);
				delete[]p_animations;
			}
			p_animationCount = animIndex + 1;
			p_animations = newAnimArray;
			memset(&p_animations[animIndex], 0, sizeof(princeAnim_s));
				
			//Remove symbols we won't store as animation script name
			RemoveSpacesAtEnd(line);
			RemoveSpacesAtStart(line);
			RemoveSymbolAtEnd(line, ']');
			RemoveSymbolAtStart(line, '[');
			if(strlen(line) == 0)
			{
				StatusUpdate("Warning: Found animation script with no name");
				delete[]byteCodeBuffer;
				goto fail;
			}
			p_animations[animIndex].scriptName = new char[strlen(line) + 1];
			strcpy(p_animations[animIndex].scriptName, line);
		}
	}

	delete[]byteCodeBuffer;
	delete[]scriptData;
	scriptData = 0;

	//Handle the jumpsToFix array
	if(jumpsToFix)
	{
		for(int i = 0; i < jumpsToFixNum; i++)
		{
			char secondToken[MAXLINELENGTH] = {0};
			SeparateString(jumpsToFix[i].label, secondToken, ':');

			//Find what animation we're supposed to jump to
			int jumpToAnimIndex = -1;
			for(int j = 0; j < p_animationCount; j++)
			{
				if(_stricmp(jumpsToFix[i].label, p_animations[j].scriptName) == 0)
				{
					jumpToAnimIndex = j;
					break;
				}
			}
			if(jumpToAnimIndex == -1)
			{
				StatusUpdate("Warning: Failed to find animation script with name %s while fixing Anim script commands", jumpsToFix[i].label);
				goto fail;
			}

			//If there's a second token, then we'll need to figure out what part of a function to jump to
			int byteCodePos = 0;
			if(secondToken[0] != 0)
			{
				int labelIndex = -1;
				for(int j = 0; j < labelsFoundNum; j++)
				{
					if(jumpToAnimIndex == labelsFound[j].animIndex && _stricmp(secondToken, labelsFound[j].label) == 0)
					{
						labelIndex = j;
						break;
					}
				}
				if(labelIndex == -1)
				{
					StatusUpdate("Warning: Failed to find label with name %s while fixing Anim script commands", secondToken);
					goto fail;
				}
				byteCodePos = labelsFound[labelIndex].byteCodePos;
			}

			//Define the animation index and byte code pos to jump to
			//TODO: Check if jumpToAnimIndex and byteCodePos are both within short ranges?
			(short &) p_animations[jumpsToFix[i].animIndex].byteCode[jumpsToFix[i].byteCodePos] = (short) GetNumberFromScriptName(p_animations[jumpToAnimIndex].scriptName);
			//(short &) p_animations[jumpsToFix[i].animIndex].byteCode[jumpsToFix[i].byteCodePos + sizeof(short)] = (short) byteCodePos;
		}
	}

	//Calculate full size of animation data
	animationDataSize = 0;
	for(int j = 0; j < p_animationCount; j++)
		animationDataSize += p_animations[j].byteCodeSize + 1; //We add one for the checksum byte

	//Prepare data for new sequence.dat
	datHeader_s header;
	header.footerOffset = animationDataSize + sizeof(header); //Offset of masterIndex -- after this header and after all DAT entries data
	header.footerSize = sizeof(datMasterIndex_s) + sizeof(datFooterHeader_s) + sizeof(datFooter_s) + (sizeof(datFooterEntryV2_s) * p_animationCount); //Size of master index + footer headers + footers + footer entries
	datMasterIndex_s masterIndex;
	masterIndex.footerCount = 1; //Quantity of footer headers. There's only one in sequence.dat
	datFooterHeader_s footerHeader;
	footerHeader.footerOffset = sizeof(datMasterIndex_s) + sizeof(datFooterHeader_s);
	memcpy(footerHeader.magic, "SQES", 4);
	datFooter_s footer;
	footer.entryCount = p_animationCount; //Quantity of animations
	footerEntries = new datFooterEntryV2_s[p_animationCount];
	currentPos = sizeof(datHeader_s);
	for(int i = 0; i < p_animationCount; i++)
	{
		datFooterEntryV2_s *entry = &footerEntries[i];
		entry->flags[0] = 64; //No idea what these flags mean, but this is what their values are by default
		entry->flags[1] = 0;
		entry->flags[2] = 0;

		entry->id = (unsigned short) GetNumberFromScriptName(p_animations[i].scriptName);
		entry->offset = currentPos;
		entry->size = p_animations[i].byteCodeSize;
		currentPos += p_animations[i].byteCodeSize + 1; //Animation size and checksum byte
	}

	//Write all data to a new sequence.dat
	fopen_s(&file, "sequence.dat", "wb");
	if(!file)
	{
		StatusUpdate("Warning: Failed to open sequence.dat for writing");
		goto fail;
	}
	fwrite(&header, sizeof(datHeader_s), 1, file); //Header with footer offset
	for(int i = 0; i < p_animationCount; i++)
	{
		//Calculate checksum byte
		unsigned char checksum = 0;
		for(unsigned int k = 0; k < p_animations[i].byteCodeSize; k++)
			checksum += p_animations[i].byteCode[k];
		checksum = 0xFF - checksum;
		fwrite(&checksum, 1, 1, file); //Checksum
		fwrite(p_animations[i].byteCode, p_animations[i].byteCodeSize, 1, file); //DAT file data (animations)
	}
	fwrite(&masterIndex, sizeof(datMasterIndex_s), 1, file); //Master index with footer header count
	fwrite(&footerHeader, sizeof(datFooterHeader_s), 1, file); //Footer header (we only have one)
	fwrite(&footer, sizeof(datFooter_s), 1, file); //Footer (we only have one)
	fwrite(footerEntries, sizeof(datFooterEntryV2_s), p_animationCount, file); //Footer entries
	fclose(file);
	StatusUpdate("Wrote sequence.dat");

	//Finish
fail:
	if(scriptData)
		delete[]scriptData;
	if(p_animations)
		delete[]p_animations;
	if(jumpsToFix)
	{
		for(int i = 0; i < jumpsToFixNum; i++)
			delete[]jumpsToFix[i].label;
		delete[]jumpsToFix;
	}
	if(labelsFound)
	{
		for(int i = 0; i < labelsFoundNum; i++)
			delete[]labelsFound[i].label;
		delete[]labelsFound;
	}
	if(footerEntries)
		delete[]footerEntries;
	return 1;
}