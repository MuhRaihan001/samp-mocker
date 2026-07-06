#include "amx_native_utils.h"

AMX_HEADER *GetHeader(AMX *amx) {
    return (AMX_HEADER *)amx->base;
}

bool UsesNameTable(AMX_HEADER *hdr) {
    return hdr->defsize == sizeof(AmxNativeEntryNT);
}

int GetNativeCount(AMX *amx) {
    AMX_HEADER *hdr = GetHeader(amx);
    if (hdr->defsize <= 0) return 0;
    return (int)((hdr->libraries - hdr->natives) / hdr->defsize);
}

bool GetNativeInfo(AMX *amx, int index, ucell *outAddress, std::string *outName) {
    AMX_HEADER *hdr = GetHeader(amx);
    int count = GetNativeCount(amx);
    if (index < 0 || index >= count) return false;

    unsigned char *base = (unsigned char *)amx->base;
    unsigned char *entry = base + hdr->natives + index * hdr->defsize;

    if (UsesNameTable(hdr)) {
        AmxNativeEntryNT *e = (AmxNativeEntryNT *)entry;
        *outAddress = e->address;
        *outName = std::string((char *)(base + e->nameofs));
    } else {
        AmxNativeEntryOld *e = (AmxNativeEntryOld *)entry;
        *outAddress = e->address;
        *outName = std::string(e->name);
    }
    return true;
}

bool SetNativeAddress(AMX *amx, int index, ucell newAddress) {
    AMX_HEADER *hdr = GetHeader(amx);
    int count = GetNativeCount(amx);
    if (index < 0 || index >= count) return false;

    unsigned char *base = (unsigned char *)amx->base;
    unsigned char *entry = base + hdr->natives + index * hdr->defsize;

    if (UsesNameTable(hdr)) {
        ((AmxNativeEntryNT *)entry)->address = newAddress;
    } else {
        ((AmxNativeEntryOld *)entry)->address = newAddress;
    }
    return true;
}

int FindNativeIndexByName(AMX *amx, const std::string &name) {
    int count = GetNativeCount(amx);
    for (int i = 0; i < count; i++) {
        ucell addr;
        std::string entryName;
        if (GetNativeInfo(amx, i, &addr, &entryName) && entryName == name) {
            return i;
        }
    }
    return -1;
}
