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

// Baca address + name dari entry ke-i, apapun formatnya.
bool GetNativeInfo(AMX *amx, int index, ucell *outAddress, std::string *outName);

// Tulis address baru ke entry ke-i (buat mocking / restore).
bool SetNativeAddress(AMX *amx, int index, ucell newAddress);

int FindNativeIndexByName(AMX *amx, const std::string &name);
