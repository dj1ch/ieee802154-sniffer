/* Basic console example (esp_console_repl API)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "cmd_system.h"
#include "esp_ieee802154.h"
#include "cmd_nvs.h"

static const char* TAG = "ieee802154_sniffer";
#define PROMPT_STR CONFIG_IDF_TARGET

/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_CONSOLE_STORE_HISTORY

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"

static void initialize_filesystem(void)
{
    static wl_handle_t wl_handle;
    const esp_vfs_fat_mount_config_t mount_config = {
            .max_files = 4,
            .format_if_mount_failed = true
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(MOUNT_PATH, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}
#endif // CONFIG_STORE_HISTORY

/**
 * Set channel for the sniffer to use
 */
static int channel = 11;

/**
 * Basic sniffing callback
 */
static void ieee802154_rx_cb(const uint8_t *frame, uint8_t len) {
    printf("Packet length: %d\n", len);
    for (int i = 0; i < len; i++) {
        printf("%02x ", frame[i]);
    }
    printf("\n");
}

/**
 * Registers everything needed to start the sniffer
 */
static void register_sniffer(void) {
    esp_ieee802154_enable();
    esp_ieee802154_set_promiscuous(true);

    register_sniffer_commands();
}

/**
 * Stops the sniffing callback
 */
static void stop_sniffer(void) {
    esp_ieee802154_set_receive_callback(NULL);
}

/**
 * Starts sniffing callback using ieee802154_rx_cb()
 */
static void start_sniffer(void) {
    esp_ieee802154_set_receive_callback(ieee802154_rx_cb);
    esp_ieee802154_set_channel(channel);
}

/**
 * Sets channel for sniffing.
 * However, you must reload the sniffer in order to use this channel.
 */
static void set_channel(int c) {
    channel = c;
    printf("Set channel to %d", channel);
}

/**
 * Command to start the sniffer
 */
static int cmd_start_sniffer(int argc, char **argv) {
    start_sniffer();
    printf("Sniffer started\n");
    return 0;
}

/**
 * Command to stop the sniffer
 */
static int cmd_stop_sniffer(int argc, char **argv) {
    stop_sniffer();
    printf("Sniffer stopped\n");
    return 0;
}

/**
 * Command to set the channel
 */
static int cmd_set_channel(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: set_channel <channel>\n");
        return 1;
    }
    int channel = atoi(argv[1]);
    if (channel < 11 || channel > 26) {
        printf("Channel must be between 11 and 26 for IEEE 802.15.4\n");
        return 1;
    }
    set_channel(channel);
    printf("Channel set to %d\n", channel);
    return 0;
}

/**
 * Registers the commands ONLY
 */
static void register_sniffer_commands(void) {
    const esp_console_cmd_t start_sniffer_cmd = {
        .command = "start_sniffer",
        .help = "Start the IEEE 802.15.4 sniffer",
        .hint = NULL,
        .func = &cmd_start_sniffer,
    };

    const esp_console_cmd_t stop_sniffer_cmd = {
        .command = "stop_sniffer",
        .help = "Stop the IEEE 802.15.4 sniffer",
        .hint = NULL,
        .func = &cmd_stop_sniffer,
    };

    const esp_console_cmd_t set_channel_cmd = {
        .command = "set_channel",
        .help = "Set the sniffer channel (11-26)",
        .hint = NULL,
        .func = &cmd_set_channel,
    };

    esp_console_cmd_register(&start_sniffer_cmd);
    esp_console_cmd_register(&stop_sniffer_cmd);
    esp_console_cmd_register(&set_channel_cmd);
}


static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void app_main(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH;

    initialize_nvs();

#if CONFIG_CONSOLE_STORE_HISTORY
    initialize_filesystem();
    repl_config.history_save_path = HISTORY_PATH;
    ESP_LOGI(TAG, "Command history enabled");
#else
    ESP_LOGI(TAG, "Command history disabled");
#endif

    /* Register commands */
    esp_console_register_help_command();
    register_system();
    register_nvs();
    register_sniffer();

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

#else
#error Unsupported console type
#endif

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
