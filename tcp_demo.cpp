#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "lwip/apps/lwiperf.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "ArduinoJson.h"

#include <lwip/sockets.h>

namespace json = ArduinoJson;

#define TEST_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)

static void send_message(int socket, char const *msg) {
    int len = strlen(msg);
    int done = 0;
    while (done < len) {
        int done_now = send(socket, msg + done, len - done, 0);
        if (done_now <= 0)
            return;

        done += done_now;
    }
}

static int handle_single_command(int conn_sock) {
    char rx_buff[128];
    int done = 0;
    //send_message(conn_sock, "Enter command: ");
    while (done < sizeof(rx_buff)) {
        int done_now = recv(conn_sock, rx_buff + done, sizeof(rx_buff) - done, 0);
        if (done_now <= 0)
            return -1;

        done += done_now;
        char *end = strnstr(rx_buff, "\0", done);
        if (!end)
            continue;

        auto rx_JSON_value = json::JsonDocument();
        json::DeserializationError error = json::deserializeJson(rx_JSON_value, rx_buff);

        auto tx_JSON_value = json::JsonDocument();
        //*tx_buff = NULL;
        int buff_len = 0;

        if (error) {
            printf("Error json parse. %s", error.c_str());
        } else {
            for (json::JsonVariant command : rx_JSON_value.as<json::JsonArray>()) {
                char const *command_name = command["cmd"];
                 printf("Command Found: %s", command_name);
                auto pars = command["pars"];

                send_message(conn_sock, command_name);
                send_message(conn_sock, "\n\r");
    
                // auto ans = cmd_execute(command_name, pars);
                // tx_JSON_value[command_name] = ans;
            }

            // buff_len = json::measureJson(tx_JSON_value); /* returns 0 on fail */
            // buff_len++;
            // *tx_buff = new char[buff_len];
            // if (!(*tx_buff)) {
            //     lDebug_uart_semihost(Error, "Out Of Memory");
            //     buff_len = 0;
            // } else {
            //     json::serializeJson(tx_JSON_value, *tx_buff, buff_len);
            //     lDebug_uart_semihost(Info, "%s", *tx_buff);
            // }
        }
        
        break;
    }

    return 0;
}

const int kConnectionThreadCount = 3;
static xSemaphoreHandle s_ConnectionSemaphore;

static void do_handle_connection(void *arg) {
    int conn_sock = (int)arg;
    while (!handle_single_command(conn_sock)) {
    }

    closesocket(conn_sock);
    xSemaphoreGive(s_ConnectionSemaphore);
    vTaskDelete(NULL);
}

static void handle_connection(int conn_sock) {
    TaskHandle_t task;
    xSemaphoreTake(s_ConnectionSemaphore, portMAX_DELAY);
    xTaskCreate(do_handle_connection, "Connection Thread", configMINIMAL_STACK_SIZE, (void *)conn_sock, TEST_TASK_PRIORITY, &task);
}

static void run_server() {
    int server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    struct sockaddr_in listen_addr =
        {
            .sin_len = sizeof(struct sockaddr_in),
            .sin_family = AF_INET,
            .sin_port = htons(1234),
            .sin_addr = 0,
        };
    
    if (server_sock < 0) {
        printf("Unable to create socket: error %d", errno);
        return;
    }

    if (bind(server_sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
        printf("Unable to bind socket: error %d\n", errno);
        return;
    }

    if (listen(server_sock, kConnectionThreadCount * 2) < 0) {
        printf("Unable to listen on socket: error %d\n", errno);
        return;
    }

    printf("Starting server at %s on port %u\n", ip4addr_ntoa(netif_ip4_addr(netif_list)), ntohs(listen_addr.sin_port));

    while (true) {
        struct sockaddr_storage remote_addr;
        socklen_t len = sizeof(remote_addr);
        int conn_sock = accept(server_sock, (struct sockaddr *)&remote_addr, &len);
        if (conn_sock >= 0)
            handle_connection(conn_sock);
    }
}

static void main_task(__unused void *params) {
    if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return;
    }

    cyw43_arch_enable_sta_mode();
    printf("Connecting to WiFi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect.\n");
        exit(1);
    } else {
        printf("Connected.\n");
    }

    run_server();
    cyw43_arch_deinit();
    vTaskDelete(NULL);
}

int main(void) {
    stdio_init_all();

    TaskHandle_t task;
    xTaskCreate(main_task, "MainThread", configMINIMAL_STACK_SIZE, NULL, TEST_TASK_PRIORITY, &task);
    s_ConnectionSemaphore = xSemaphoreCreateCounting(kConnectionThreadCount, kConnectionThreadCount);

    vTaskStartScheduler();
}
