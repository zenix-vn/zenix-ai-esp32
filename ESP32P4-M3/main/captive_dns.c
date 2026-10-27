#include "captive_dns.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

static const char *TAG = "dns";

static void dns_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(53), .sin_addr.s_addr = htonl(INADDR_ANY)};
    if (sock < 0 || bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Cannot bind :53");
        vTaskDelete(NULL);
    }
    static const uint8_t answer_ip[4] = {192, 168, 4, 1};
    uint8_t buf[512];
    while (true) {
        struct sockaddr_in from;
        socklen_t fl = sizeof(from);
        int len = recvfrom(sock, buf, sizeof(buf) - 16, 0, (struct sockaddr *)&from, &fl);
        if (len < 12) {
            continue;
        }
        // Tim cuoi phan question (QNAME + QTYPE + QCLASS).
        int p = 12;
        while (p < len && buf[p] != 0) {
            p += buf[p] + 1;
        }
        if (p + 5 > len) {
            continue;
        }
        uint16_t qtype = (buf[p + 1] << 8) | buf[p + 2];
        int qend = p + 5;

        buf[2] = 0x81;  // QR=1, RD=1
        buf[3] = 0x80;  // RA=1, NOERROR
        buf[6] = 0;     // ANCOUNT
        buf[7] = qtype == 1 ? 1 : 0;
        buf[8] = buf[9] = buf[10] = buf[11] = 0;
        int out = qend;
        if (qtype == 1) {
            const uint8_t rr[] = {0xc0, 0x0c, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4};
            memcpy(buf + out, rr, sizeof(rr));
            out += sizeof(rr);
            memcpy(buf + out, answer_ip, 4);
            out += 4;
        }
        sendto(sock, buf, out, 0, (struct sockaddr *)&from, fl);
    }
}

void captive_dns_start(void)
{
    xTaskCreate(dns_task, "dns", 4096, NULL, 4, NULL);
}
