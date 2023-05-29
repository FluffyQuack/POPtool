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

bool Prince_OpenDAT(const char *path, int *entryCount = 0);
bool Prince_OpenDATv2(const char *path, int *entryCount = 0);
bool Prince_CloseDAT();
bool Prince_LoadEntryFromDAT(unsigned char **data, unsigned int *size, int loadEntryIdx = -1, int loadEntryId = -1, unsigned short *entryId = 0);
bool Prince_LoadEntryFromDATv2(unsigned char **data, unsigned int *size, int type, int loadEntryIdx = -1, int loadEntryId = -1, unsigned short *entryId = 0, unsigned char *flags = 0);
int Prince_ReturnFileTypeCountFromDAT(int type);