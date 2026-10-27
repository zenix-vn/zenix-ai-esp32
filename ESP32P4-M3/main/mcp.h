#pragma once

#include "cJSON.h"

// Xu ly mot JSON-RPC payload MCP tu server va gui phan hoi (neu can).
void mcp_handle(const cJSON *payload);
