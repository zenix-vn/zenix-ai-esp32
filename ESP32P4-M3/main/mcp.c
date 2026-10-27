#include "mcp.h"

#include <stdio.h>
#include <string.h>
#include "audio.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "net_wifi.h"
#include "xz_protocol.h"

static const char *TAG = "mcp";

static void send_payload(cJSON *payload)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "type", "mcp");
    cJSON_AddItemToObject(msg, "payload", payload);
    xz_send_json(msg);
    cJSON_Delete(msg);
}

static cJSON *new_response(const cJSON *id)
{
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "jsonrpc", "2.0");
    cJSON_AddItemToObject(r, "id", cJSON_Duplicate(id, true));
    return r;
}

static void reply_error(const cJSON *id, int code, const char *message)
{
    cJSON *r = new_response(id);
    cJSON *e = cJSON_AddObjectToObject(r, "error");
    cJSON_AddNumberToObject(e, "code", code);
    cJSON_AddStringToObject(e, "message", message);
    send_payload(r);
}

static void reply_text(const cJSON *id, const char *text)
{
    cJSON *r = new_response(id);
    cJSON *result = cJSON_AddObjectToObject(r, "result");
    cJSON *content = cJSON_AddArrayToObject(result, "content");
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "type", "text");
    cJSON_AddStringToObject(item, "text", text);
    cJSON_AddItemToArray(content, item);
    cJSON_AddBoolToObject(result, "isError", false);
    send_payload(r);
}

static void add_tool(cJSON *tools, const char *name, const char *desc, cJSON *properties, const char *required)
{
    cJSON *t = cJSON_CreateObject();
    cJSON_AddStringToObject(t, "name", name);
    cJSON_AddStringToObject(t, "description", desc);
    cJSON *schema = cJSON_AddObjectToObject(t, "inputSchema");
    cJSON_AddStringToObject(schema, "type", "object");
    cJSON_AddItemToObject(schema, "properties", properties);
    if (required) {
        cJSON *req = cJSON_AddArrayToObject(schema, "required");
        cJSON_AddItemToArray(req, cJSON_CreateString(required));
    }
    cJSON_AddItemToArray(tools, t);
}

static void list_tools(const cJSON *id)
{
    cJSON *r = new_response(id);
    cJSON *result = cJSON_AddObjectToObject(r, "result");
    cJSON *tools = cJSON_AddArrayToObject(result, "tools");

    add_tool(tools, "self.get_device_status",
             "Get the device status: speaker volume, Wi-Fi signal strength and free memory. "
             "Call this before changing settings or when the user asks about the device.",
             cJSON_CreateObject(), NULL);

    cJSON *props = cJSON_CreateObject();
    cJSON *vol = cJSON_AddObjectToObject(props, "volume");
    cJSON_AddStringToObject(vol, "type", "integer");
    cJSON_AddNumberToObject(vol, "minimum", 0);
    cJSON_AddNumberToObject(vol, "maximum", 100);
    add_tool(tools, "self.audio_speaker.set_volume", "Set the speaker volume (0-100).", props, "volume");

    cJSON_AddStringToObject(result, "nextCursor", "");
    send_payload(r);
}

static void call_tool(const cJSON *id, const cJSON *params)
{
    const cJSON *name = cJSON_GetObjectItem(params, "name");
    const cJSON *args = cJSON_GetObjectItem(params, "arguments");
    if (!cJSON_IsString(name)) {
        reply_error(id, -32602, "Missing tool name");
        return;
    }
    ESP_LOGI(TAG, "tools/call %s", name->valuestring);

    if (strcmp(name->valuestring, "self.get_device_status") == 0) {
        char buf[160];
        snprintf(buf, sizeof(buf), "{\"volume\":%d,\"wifi_rssi\":%d,\"free_heap\":%u,\"firmware\":\"%s\"}",
                 audio_get_volume(), wifi_get_rssi(), (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
                 esp_app_get_description()->version);
        reply_text(id, buf);
    } else if (strcmp(name->valuestring, "self.audio_speaker.set_volume") == 0) {
        const cJSON *v = cJSON_GetObjectItem(args, "volume");
        if (!cJSON_IsNumber(v)) {
            reply_error(id, -32602, "Missing integer argument: volume");
            return;
        }
        audio_set_volume(v->valueint);
        reply_text(id, "true");
    } else {
        reply_error(id, -32601, "Unknown tool");
    }
}

void mcp_handle(const cJSON *payload)
{
    const cJSON *method = cJSON_GetObjectItem(payload, "method");
    const cJSON *id = cJSON_GetObjectItem(payload, "id");
    if (!cJSON_IsString(method) || !id) {
        return;  // response hoac notification: khong can tra loi
    }

    if (strcmp(method->valuestring, "initialize") == 0) {
        cJSON *r = new_response(id);
        cJSON *result = cJSON_AddObjectToObject(r, "result");
        cJSON_AddStringToObject(result, "protocolVersion", "2024-11-05");
        cJSON *caps = cJSON_AddObjectToObject(result, "capabilities");
        cJSON_AddObjectToObject(caps, "tools");
        cJSON *info = cJSON_AddObjectToObject(result, "serverInfo");
        cJSON_AddStringToObject(info, "name", CONFIG_XZ_BOARD_NAME);
        cJSON_AddStringToObject(info, "version", esp_app_get_description()->version);
        send_payload(r);
    } else if (strcmp(method->valuestring, "tools/list") == 0) {
        list_tools(id);
    } else if (strcmp(method->valuestring, "tools/call") == 0) {
        call_tool(id, cJSON_GetObjectItem(payload, "params"));
    } else {
        reply_error(id, -32601, "Method not found");
    }
}
