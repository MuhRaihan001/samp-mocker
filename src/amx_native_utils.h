#pragma once

#include <string>
#include "../sdk/amx/amx.h"

#ifndef sNAMEMAX
  #define sNAMEMAX 31
#endif

struct AmxNativeEntryOld {
    ucell address;
    char  name[sNAMEMAX + 1];
};

struct AmxNativeEntryNT {
    ucell address;
    ucell nameofs;
};

AMX_HEADER *GetHeader(AMX *amx);
bool UsesNameTable(AMX_HEADER *hdr);
int GetNativeCount(AMX *amx);

// Reads the address + name from the i-th entry, regardless of format.
bool GetNativeInfo(AMX *amx, int index, ucell *outAddress, std::string *outName);

// Writes a new address to the i-th entry (for mocking / restoring).
bool SetNativeAddress(AMX *amx, int index, ucell newAddress);

int FindNativeIndexByName(AMX *amx, const std::string &name);