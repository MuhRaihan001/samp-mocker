#pragma once

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include "../sdk/amx/amx.h"
#include "../json.hpp"

using json = nlohmann::json;

const int MAX_MOCKED_NATIVES = 512;

struct MockSlot {
    bool        used = false;
    bool        active = false;
    std::string name;
    ucell       originalAddress = 0;
    bool        hasReturnOverride = false;
    cell        returnOverride = 0;
    std::vector<json> callLog;
    std::vector<json> writes;
    std::vector<std::string> paramTypes;
};

extern MockSlot g_mockSlots[MAX_MOCKED_NATIVES];
extern std::unordered_map<std::string, int> g_nameToSlot;
extern const std::array<AMX_NATIVE, MAX_MOCKED_NATIVES> g_trampolines;

int AllocMockSlot(const std::string &name);
void ResetAllMocks(AMX *amx);