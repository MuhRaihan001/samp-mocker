#include "mock_native.h"
#include "amx_native_utils.h"
#include <utility>

MockSlot g_mockSlots[MAX_MOCKED_NATIVES];
std::unordered_map<std::string, int> g_nameToSlot;

// Generic per-slot trampoline. Only used within this TU to build g_trampolines,
template <std::size_t N>
static cell AMX_NATIVE_CALL MockTrampoline(AMX *amx, cell *params) {
    MockSlot &slot = g_mockSlots[N];

    int argc = (int)(params[0] / sizeof(cell));
    json args = json::array();
    for (int i = 1; i <= argc; i++) {
        std::string type = (i - 1 < (int)slot.paramTypes.size()) ? slot.paramTypes[i - 1] : "i";

        if (type == "s") {
            cell *physAddr = nullptr;
            amx_GetAddr(amx, params[i], &physAddr);
            if (physAddr) {
                int len = 0;
                amx_StrLen(physAddr, &len);
                std::string val(len, '\0');
                amx_GetString(&val[0], physAddr, 0, len + 1);
                args.push_back(val);
            } else {
                args.push_back(nullptr);
            }
        } else if (type == "f") {
            args.push_back(amx_ctof(params[i]));
        } else {
            args.push_back((long long)params[i]);
        }
    }
    slot.callLog.push_back(args);

    if (slot.active) {
        for (const auto &w : slot.writes) {
            int paramIndex = w.value("param", 0);
            if (paramIndex < 1 || paramIndex > argc) continue;

            cell *physAddr = nullptr;
            amx_GetAddr(amx, params[paramIndex], &physAddr);
            if (!physAddr) continue;

            std::string type = w.value("type", "i");
            if (type == "s") {
                std::string val = w.value("value", std::string());
                amx_SetString(physAddr, val.c_str(), 0, 0, val.size() + 1);
            } else if (type == "f") {
                float f = w.value("value", 0.0f);
                *physAddr = amx_ftoc(f);
            } else {
                int i = w.value("value", 0);
                *physAddr = (cell)i;
            }
        }
        return slot.hasReturnOverride ? slot.returnOverride : 0;
    }

    if (slot.originalAddress) {
        AMX_NATIVE original = (AMX_NATIVE)slot.originalAddress;
        return original(amx, params);
    }
    return 0;
}

template <std::size_t... I>
static constexpr std::array<AMX_NATIVE, sizeof...(I)> MakeTrampolineTable(std::index_sequence<I...>) {
    return { &MockTrampoline<I>... };
}

const std::array<AMX_NATIVE, MAX_MOCKED_NATIVES> g_trampolines =
    MakeTrampolineTable(std::make_index_sequence<MAX_MOCKED_NATIVES>{});

int AllocMockSlot(const std::string &name) {
    auto it = g_nameToSlot.find(name);
    if (it != g_nameToSlot.end()) return it->second;

    for (int i = 0; i < MAX_MOCKED_NATIVES; i++) {
        if (!g_mockSlots[i].used) {
            g_mockSlots[i].used = true;
            g_mockSlots[i].name = name;
            g_nameToSlot[name] = i;
            return i;
        }
    }
    return -1;
}

void ResetAllMocks(AMX *amx) {
    for (int i = 0; i < MAX_MOCKED_NATIVES; i++) {
        MockSlot &slot = g_mockSlots[i];
        if (!slot.used) continue;
        if (amx && slot.originalAddress) {
            int idx = FindNativeIndexByName(amx, slot.name);
            if (idx >= 0) SetNativeAddress(amx, idx, slot.originalAddress);
        }
        slot = MockSlot{};
    }
    g_nameToSlot.clear();
}