#pragma once

#include "../json.hpp"

using json = nlohmann::json;

json HandleInvoke(const json &cmd);
json HandleGetVar(const json &cmd);
json HandleSetVar(const json &cmd);
json HandleMockNative(const json &cmd);
json HandleUnmockNative(const json &cmd);
json HandleGetNativeCalls(const json &cmd);
json HandleResetMocks(const json &cmd);
json HandleSetConfig(const json &cmd);
json HandleGetConfig(const json &cmd);

json Dispatch(const json &cmd);
