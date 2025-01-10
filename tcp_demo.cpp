#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "lwip/apps/lwiperf.h"
#include "lwip/apps/fs.h"
#include <lwip/apps/httpd.h>
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "ArduinoJson.h"

#include <lwip/sockets.h>

#include "tcp_server_command.h"

#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"

#include "cgi_example.h"
#include "ssi_example.h"

#define DEBUG_printf printf

#define MAIN_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)

//#define MQTT_SERVER_HOST "telemetria"
#define MQTT_SERVER_PORT 1883

#define PUBLISH_TOPIC  "/pruebas/texto"

#define WILL_TOPIC "/pruebas/uvyt"        /*uvyt: ultima voluntad y testamentp*/
#define WILL_MSG "sin conexion a broker"          /*uvyt: ultima voluntad y testamentp*/

#define MS_PUBLISH_PERIOD 5000
#define MQTT_TLS 0 // needs to be 1 for AWS IoT

typedef struct MQTT_CLIENT_T_ {
    ip_addr_t remote_addr;
    mqtt_client_t *mqtt_client;
    u8_t receiving;
    u32_t received;
    u32_t counter;
    u32_t reconnect;
} MQTT_CLIENT_T;

tcp_server_command cmd_server(456);
//tcp_server_command otro_server(678);

// Perform initialisation
static MQTT_CLIENT_T* mqtt_client_init(void)
{ 
    MQTT_CLIENT_T *client = static_cast<MQTT_CLIENT_T *>(calloc(1, sizeof(MQTT_CLIENT_T)));
    
    if (!client) {
        DEBUG_printf("failed to allocate state\n");
        return NULL;
    }
    client->receiving = 0;
    client->received = 0;
    return client;
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    if (status != 0) {
        DEBUG_printf("Error during connection: err %d.\n", status);
    } else {
        DEBUG_printf("MQTT connected.\n");
    }
}

/* 
 * Called when publish is complete either with success or failure
 */
void mqtt_pub_request_cb(void *arg, err_t err) {
    MQTT_CLIENT_T *client = (MQTT_CLIENT_T *)arg;
    printf("request publish ERROR %d \n", err);
    client->receiving = 0;
    client->received++;
}

/*
 * The app publishing
 */
err_t mqtt_app_publish(MQTT_CLIENT_T *client, char *payload)
{
    err_t err;
    u8_t qos = 0;       /* 0 1 or 2, see MQTT specification */
    u8_t retain = 0;

    // cyw43_arch_lwip_begin();
    // sleep_ms(2);
    err = mqtt_publish(client->mqtt_client, PUBLISH_TOPIC , payload, strlen(payload), qos, retain, mqtt_pub_request_cb, client);

    // cyw43_arch_lwip_end();

    if(err != ERR_OK) {
        DEBUG_printf("**** Publish fail***** %d\n", err);
        cyw43_arch_lwip_end();
        sleep_ms(10);
        cyw43_arch_lwip_begin();
        sleep_ms(10);
    }
    return err;
}

/* Initiate client and connect to server, if this fails immediately an error code is returned
 * otherwise mqtt_connection_cb will be called with connection result after attempting to
 * to establish a connection with the server. For now MQTT version 3.1.1 is always used
 */
err_t mqtt_app_connect(MQTT_CLIENT_T *client)
{
    struct mqtt_connect_client_info_t ci;
    err_t err;

    memset(&ci, 0, sizeof(ci));

    ci.client_id = "PicoW";
    ci.client_user = NULL;
    ci.client_pass = NULL;
    ci.keep_alive = 0;
    ci.will_topic = WILL_TOPIC;
    ci.will_msg = WILL_MSG;
    ci.will_retain = 0;
    ci.will_qos = 2;

    err = mqtt_client_connect(client->mqtt_client, &(client->remote_addr), MQTT_SERVER_PORT, mqtt_connection_cb, client, &ci);

    if (err != ERR_OK) {
        DEBUG_printf("ERROR en mqtt_app_connect() %d \n",err);
    }

    return err;
}

void perform_payload( char *p)
{
    time_t s;
    struct tm* current_time;

    // time in seconds
    s = time(NULL);
     // to get current time
    current_time = localtime(&s);
 
    sprintf( p, "%02d:%02d:%02d",
           current_time->tm_hour,
           current_time->tm_min,
           current_time->tm_sec
           );
}

//*******************************************************************
void publish_task(void *pvParameters)
{
    MQTT_CLIENT_T *client = (MQTT_CLIENT_T *)pvParameters;
    int cont= 0;
    int aux;
    err_t error;
    char payload[16];

    size_t heap; 

    DEBUG_printf("Inicio publish_task\n");

    MQTT_CLIENT_T *my_mqtt_client = mqtt_client_init();

    IP4_ADDR(&my_mqtt_client->remote_addr, 192, 168, 137, 243);

    //run_dns_lookup(my_mqtt_client); 
    
    my_mqtt_client->mqtt_client = mqtt_client_new();
    my_mqtt_client->counter = 0;

    if (my_mqtt_client->mqtt_client == NULL) {
        DEBUG_printf("Failed to create new mqtt client\n");
    }

    error = mqtt_app_connect(my_mqtt_client);
    if ( error == ERR_OK) {
        DEBUG_printf("mqtt_app_connect() OK\n");
    }
    else{
        DEBUG_printf("mqtt_app_connect() ERROR=%d\n", error);
    }        

    while(true) {       
        aux= mqtt_client_is_connected(client->mqtt_client);
        if (aux){
             perform_payload( payload );
            //sprintf(payload,"%05d", client->counter);
            
            error = mqtt_app_publish(client,payload);   // <-- Publish!
        
            if ( error == ERR_OK) {
                DEBUG_printf("%s publicacion: %d\n", payload, client->counter);
                client->counter++;
            }
        } 

        vTaskDelay(10000); 
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

    cmd_server.start();
    //otro_server.start();
    //cyw43_arch_deinit();    
    httpd_init();
    cgi_ex_init();
    ssi_ex_init();

    MQTT_CLIENT_T *my_mqtt_client = mqtt_client_init();

    IP4_ADDR(&my_mqtt_client->remote_addr, 192, 168, 137, 243);

    my_mqtt_client->mqtt_client = mqtt_client_new();
    my_mqtt_client->counter = 0;

    if (my_mqtt_client->mqtt_client == NULL) {
        DEBUG_printf("Failed to create new mqtt client\n");
    }

    error_t error = mqtt_app_connect(my_mqtt_client);
    if ( error == ERR_OK) {
        DEBUG_printf("mqtt_app_connect() OK\n");
    }
    else{
        DEBUG_printf("mqtt_app_connect() ERROR=%d\n", error);
    }

        xTaskCreate(
        publish_task,           // Task to be run
        "PUBLISH_TASK",         // Name of the Task for debugging and managing its Task Handle
        2048,                   // Stack depth to be allocated for use with task's stack (see docs)
        my_mqtt_client,         // Arguments needed by the Task (NULL because we don't have any)
        6, // Task Priority - Higher the number the more priority [max is (configMAX_PRIORITIES - 1) provided in FreeRTOSConfig.h]
        NULL                    // Task Handle if available for managing the task
    );

    vTaskDelete(NULL);
}

int main(void) {
    stdio_init_all();

    TaskHandle_t task;
    xTaskCreate(main_task, "MainThread", configMINIMAL_STACK_SIZE, NULL, MAIN_TASK_PRIORITY, &task);    
    vTaskStartScheduler();
}
