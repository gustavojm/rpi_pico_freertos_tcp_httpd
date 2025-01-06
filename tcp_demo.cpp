#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "lwip/apps/lwiperf.h"
#include "lwip/apps/fs.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "ArduinoJson.h"

#include <lwip/apps/httpd.h>
#include <lwip/sockets.h>

#include "tcp_server_command.h"

#define MAIN_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)

tcp_server_command cmd_server(456);
//tcp_server_command otro_server(678);

static const char *cgi_handler_basic(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);

static const tCGI cgi_handlers[] = {
  {
    "/basic_cgi",
    cgi_handler_basic
  },
  {
    "/basic_cgi_2",
    cgi_handler_basic
  }
};

void
cgi_ex_init(void)
{
  http_set_cgi_handlers(cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));
}

/** This basic CGI function can parse param/value pairs and return an url that
 * is sent as a response by httpd.
 *
 * This example function just checks that the input url has two key value
 * parameter pairs: "foo=bar" and "test=123"
 * If not, it returns 404
 */
static const char *
cgi_handler_basic(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
  LWIP_ASSERT("check index", iIndex < LWIP_ARRAYSIZE(cgi_handlers));

  if (iNumParams == 2) {
    if (!strcmp(pcParam[0], "foo")) {
      if (!strcmp(pcValue[0], "bar")) {
        if (!strcmp(pcParam[1], "test")) {
          if (!strcmp(pcValue[1], "123")) {
            return "/index.html";
          }
        }
      }
    }
  }
  return "/404.html";
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

    cmd_server.start();
    //otro_server.start();
    //cyw43_arch_deinit();    
    httpd_init();
    cgi_ex_init();
    vTaskDelete(NULL);
}

int main(void) {
    stdio_init_all();

    TaskHandle_t task;
    xTaskCreate(main_task, "MainThread", configMINIMAL_STACK_SIZE, NULL, MAIN_TASK_PRIORITY, &task);    
    vTaskStartScheduler();
}
