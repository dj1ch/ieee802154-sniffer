/*
 * ieee802154-sniffer: and yet another proof of concept sniffer using the IEEE 802.15.4 protocol
 * Copyright (C) 2024 dj1ch
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
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
void esp_ieee802154_receive_done(uint8_t* frame, esp_ieee802154_frame_info_t* frame_info) {
    ESP_LOGI(TAG, "Received frame: %d bytes\n", frame[0]);

    if (frame[1] & 0x40) {
        ESP_LOGI(TAG, "Source MAC: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
                   frame[3], frame[4], frame[5], frame[6], frame[7], frame[8], frame[9], frame[10]);
    }

    for (int i = 0; i < frame[0]; i++) {
        ESP_LOGI(TAG, "%02x ", frame[i]);
    }
    ESP_LOGI(TAG, "\n");
}


static int get_state(void) {
    return esp_ieee802154_get_state();
}

/**
 * Registers everything needed to start the sniffer
 */
static void register_sniffer(void) {
    ESP_ERROR_CHECK(esp_ieee802154_enable());
    ESP_ERROR_CHECK(esp_ieee802154_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_ieee802154_set_rx_when_idle(true));
    ESP_ERROR_CHECK(esp_ieee802154_set_channel(channel));
}

/**
 * Stops the sniffing callback
 */
static void stop_sniffer(void) {
    esp_ieee802154_state_t state = get_state();

    if (state != ESP_IEEE802154_RADIO_SLEEP) {
        ESP_ERROR_CHECK(esp_ieee802154_sleep());
    } else {
        ESP_LOGE(TAG, "Cannot stop as device is already in sleep mode");
    }
}

/**
 * Starts sniffing callback using esp_ieee802154_receive
 */
static void start_sniffer(void) {
    ESP_ERROR_CHECK(esp_ieee802154_receive());
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
    register_sniffer_commands();

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
