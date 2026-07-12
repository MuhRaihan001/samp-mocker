#include "handlers.h"
#include "globals.h"
#include "amx_native_utils.h"
#include "mock_native.h"
#include <vector>
#include <string>

json HandleInvoke(const json &cmd) {
    json response;

    if (!g_amx) {
        response["error"] = "no AMX loaded yet (is the gamemode running?)";
        return response;
    }

    std::string name = cmd.value("name", cmd.value("callback", std::string()));
    if (name.empty()) {
        response["error"] = "field 'name' (or 'callback') is empty";
        return response;
    }

    int index;
    if (amx_FindPublic(g_amx, name.c_str(), &index) != AMX_ERR_NONE) {
        response["error"] = "public function not found: " + name +
            " (make sure the function is declared 'public', not just 'stock')";
        return response;
    }

    json params = cmd.value("params", json::array());
    if (!params.is_array()) {
        response["error"] = "field 'params' must be an array of {type,value}, got type: " +
            std::string(params.type_name());
        return response;
    }
    for (const auto &p : params) {
        if (!p.is_object()) {
            response["error"] = "each element of 'params' must be an object {type,value}, got type: " +
                std::string(p.type_name());
            return response;
        }
    }

    std::vector<cell> allocatedAddrs;

    for (auto it = params.rbegin(); it != params.rend(); ++it) {
        std::string type = (*it).value("type", "i");

        if (type == "s") {
            std::string val = (*it).value("value", std::string());
            cell amxAddr;
            cell *physAddr;
            int allotErr = amx_Allot(g_amx, (int)val.size() + 1, &amxAddr, &physAddr);
            if (allotErr != AMX_ERR_NONE || !physAddr) {
                for (cell addr : allocatedAddrs) amx_Release(g_amx, addr);
                response["error"] = "amx_Allot failed for string parameter (out of AMX heap?)";
                return response;
            }
            amx_SetString(physAddr, val.c_str(), 0, 0, val.size() + 1);
            amx_Push(g_amx, amxAddr);
            allocatedAddrs.push_back(amxAddr);
        } else if (type == "f") {
            float f = (*it).value("value", 0.0f);
            amx_Push(g_amx, amx_ftoc(f));
        } else {
            int i = (*it).value("value", 0);
            amx_Push(g_amx, (cell)i);
        }
    }

    cell retval = 0;
    int err = amx_Exec(g_amx, &retval, index);

    for (cell addr : allocatedAddrs) {
        amx_Release(g_amx, addr);
    }

    if (err != AMX_ERR_NONE) {
        response["error"] = "amx_Exec failed, error code: " + std::to_string(err);
        return response;
    }

    response["ok"] = true;
    response["name"] = name;
    response["return"] = (int)retval;
    return response;
}

json HandleGetVar(const json &cmd) {
    json response;
    if (!g_amx) {
        response["error"] = "no AMX loaded yet";
        return response;
    }
    std::string name = cmd.value("name", std::string());
    if (name.empty()) {
        response["error"] = "field 'name' is empty";
        return response;
    }

    cell amxAddr;
    if (amx_FindPubVar(g_amx, name.c_str(), &amxAddr) != AMX_ERR_NONE) {
        response["error"] = "pubvar not found: " + name;
        return response;
    }

    cell *physAddr;
    amx_GetAddr(g_amx, amxAddr, &physAddr);
    if (!physAddr) {
        response["error"] = "amx_GetAddr failed for pubvar: " + name;
        return response;
    }

    std::string type = cmd.value("type", "i");
    response["ok"] = true;
    response["name"] = name;
    if (type == "f") {
        response["value"] = amx_ctof(*physAddr);
    } else {
        response["value"] = (long long)*physAddr;
    }
    return response;
}

json HandleSetVar(const json &cmd) {
    json response;
    if (!g_amx) {
        response["error"] = "no AMX loaded yet";
        return response;
    }
    std::string name = cmd.value("name", std::string());
    if (name.empty()) {
        response["error"] = "field 'name' is empty";
        return response;
    }

    cell amxAddr;
    if (amx_FindPubVar(g_amx, name.c_str(), &amxAddr) != AMX_ERR_NONE) {
        response["error"] = "pubvar not found: " + name;
        return response;
    }

    cell *physAddr;
    amx_GetAddr(g_amx, amxAddr, &physAddr);
    if (!physAddr) {
        response["error"] = "amx_GetAddr failed for pubvar: " + name;
        return response;
    }

    std::string type = cmd.value("type", "i");
    if (type == "f") {
        float f = cmd.value("value", 0.0f);
        *physAddr = amx_ftoc(f);
    } else {
        int i = cmd.value("value", 0);
        *physAddr = (cell)i;
    }

    response["ok"] = true;
    response["name"] = name;
    return response;
}

static bool ParseMockName(const json &cmd, std::string &outName, json &outError) {
    outName = cmd.value("name", std::string());
    if (outName.empty()) {
        outError["error"] = "field 'name' is empty";
        return false;
    }
    return true;
}

// No-op if the slot is already active, so the stored originalAddress is not overwritten.
static bool SetupTrampoline(AMX *amx, int nativeIdx, int slotIdx, MockSlot &slot) {
    if (slot.active) return true;

    ucell originalAddr;
    std::string unused;
    if (!GetNativeInfo(amx, nativeIdx, &originalAddr, &unused)) {
        return false;
    }
    slot.originalAddress = originalAddr;
    SetNativeAddress(amx, nativeIdx, (ucell)g_trampolines[slotIdx]);
    return true;
}

static void ApplyMockConfig(const json &cmd, MockSlot &slot) {
    slot.callLog.clear();

    slot.hasReturnOverride = cmd.contains("returnValue");
    slot.returnOverride = (cell)cmd.value("returnValue", 0);

    slot.writes = (cmd.contains("writes") && cmd["writes"].is_array())
        ? cmd["writes"].get<std::vector<json>>()
        : std::vector<json>{};

    slot.paramTypes = (cmd.contains("paramTypes") && cmd["paramTypes"].is_array())
        ? cmd["paramTypes"].get<std::vector<std::string>>()
        : std::vector<std::string>{};
}

json HandleMockNative(const json &cmd) {
    json response;
    if (!g_amx) {
        response["error"] = "no AMX loaded yet";
        return response;
    }

    std::string name;
    if (!ParseMockName(cmd, name, response)) {
        return response;
    }

    int nativeIdx = FindNativeIndexByName(g_amx, name);
    if (nativeIdx < 0) {
        response["error"] = "native not found: " + name;
        return response;
    }

    int slotIdx = AllocMockSlot(name);
    if (slotIdx < 0) {
        response["error"] = "mock slots full (max " + std::to_string(MAX_MOCKED_NATIVES) + ")";
        return response;
    }

    MockSlot &slot = g_mockSlots[slotIdx];
    if (!SetupTrampoline(g_amx, nativeIdx, slotIdx, slot)) {
        response["error"] = "failed to read original native info: " + name;
        return response;
    }

    slot.active = true;
    ApplyMockConfig(cmd, slot);

    response["ok"] = true;
    response["name"] = name;
    response["mocked"] = true;
    return response;
}
json HandleUnmockNative(const json &cmd) {
    json response;
    std::string name = cmd.value("name", std::string());
    auto it = g_nameToSlot.find(name);
    if (it == g_nameToSlot.end()) {
        response["error"] = "this native is not currently mocked: " + name;
        return response;
    }

    MockSlot &slot = g_mockSlots[it->second];
    if (g_amx) {
        int idx = FindNativeIndexByName(g_amx, name);
        if (idx >= 0) SetNativeAddress(g_amx, idx, slot.originalAddress);
    }
    slot = MockSlot{};
    g_nameToSlot.erase(it);

    response["ok"] = true;
    response["name"] = name;
    response["mocked"] = false;
    return response;
}

json HandleGetNativeCalls(const json &cmd) {
    json response;
    std::string name = cmd.value("name", std::string());
    auto it = g_nameToSlot.find(name);
    if (it == g_nameToSlot.end()) {
        response["error"] = "this native is not currently mocked: " + name;
        return response;
    }
    MockSlot &slot = g_mockSlots[it->second];
    response["ok"] = true;
    response["name"] = name;
    response["callCount"] = (int)slot.callLog.size();
    response["calls"] = slot.callLog; // each element: array of raw cell (int) arguments
    return response;
}

json HandleResetMocks(const json &cmd) {
    json response;
    ResetAllMocks(g_amx);
    response["ok"] = true;
    return response;
}

json HandleSetConfig(const json &cmd) {
    json response;

    if (cmd.contains("maxRecvBuffer")) {
        long long raw = cmd.value("maxRecvBuffer", (long long)0);
        if (raw <= 0) {
            response["error"] = "field 'maxRecvBuffer' must be > 0";
            return response;
        }
        g_maxRecvBuffer = (size_t)raw;
        logprintf("[PAWN-MOCKER] maxRecvBuffer changed to %zu bytes", g_maxRecvBuffer);
    }

    response["ok"] = true;
    response["maxRecvBuffer"] = (long long)g_maxRecvBuffer;
    response["serverPort"] = (long long)PAWN_MOCKER_PORT;
    return response;
}

json HandleGetConfig(const json &cmd) {
    json response;
    response["ok"] = true;
    response["maxRecvBuffer"] = (long long)g_maxRecvBuffer;
    return response;
}

json Dispatch(const json &cmd) {
    std::string action = cmd.value("action", std::string());
    if (action.empty()) {
        if (cmd.contains("callback")) action = "invoke";
    }

    if (action == "invoke")             return HandleInvoke(cmd);
    if (action == "getvar")             return HandleGetVar(cmd);
    if (action == "setvar")             return HandleSetVar(cmd);
    if (action == "mocknative")         return HandleMockNative(cmd);
    if (action == "unmocknative")       return HandleUnmockNative(cmd);
    if (action == "getnativecalls")     return HandleGetNativeCalls(cmd);
    if (action == "resetmocks")         return HandleResetMocks(cmd);
    if (action == "setconfig")          return HandleSetConfig(cmd);
    if (action == "getconfig")          return HandleGetConfig(cmd);
    if (action == "ping") {
        json r; r["ok"] = true; r["pong"] = true; return r;
    }

    json response;
    response["error"] = "unknown action: " + action;
    return response;
}