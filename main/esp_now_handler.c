#include <string.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_now_handler.h"

#include "led_strip.h" // LEDライブラリを読み込む

#define LED_GPIO_PIN 38
#define MAX_LEDS 1

static led_strip_handle_t led_strip;
static int success_count = 100; // 初期値は満点にしておく
#define HISTORY_WEIGHT 100     // どのくらいの期間の平均を取るか

static const char *TAG = "ESP_NOW_TX";

//受信側(本体)のMACアドレス
uint8_t PEER_MAC_ADDRESS[] = {0x90, 0x70, 0x69, 0x35, 0x38, 0xE8};

// 送信結果を受け取るコールバック関数（正しく送れたか確認用）
void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // 1. 成功率の簡易的な移動平均計算
    if (status == ESP_NOW_SEND_SUCCESS) {
        if (success_count < HISTORY_WEIGHT) success_count++;
    } else {
        if (success_count > 0) success_count--;
    }

    // 2. 成功率(0〜1.0)を算出
    float ratio = (float)success_count / HISTORY_WEIGHT;

    // 3. 色の合成 (緑: 0,255,0   赤: 255,0,0)
    uint32_t red   = (uint32_t)(255 * (1.0 - ratio));
    uint32_t green = (uint32_t)(255 * ratio);
    uint32_t blue  = 0;

    // 4. LEDに反映
    led_strip_set_pixel(led_strip, 0, red, green, blue);
    led_strip_refresh(led_strip);
}

// LEDの初期化関数
void led_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO_PIN,
        .max_leds = MAX_LEDS,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, 
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip); 
}

// 初期化処理
void wifi_now_init(void) {
    // 1. NVSの初期化（Wi-Fi設定の保存に必要）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Wi-FiをSTAモードで起動
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 🚨【変更点①】本体の1chと同期させるため、コントローラーのWi-Fiチャネルを1chに強制固定
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));

    // 3. ESP-NOWの初期化
    ESP_ERROR_CHECK(esp_now_init());
    
    // 送信結果の通知設定
    esp_now_register_send_cb(on_data_sent);

    // 4. 送信相手（ピア）の登録
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, PEER_MAC_ADDRESS, 6);
    peerInfo.channel = 1; // 🚨【変更点②】0から「1」に変更（明示的に1chのピアとして登録）
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        ESP_LOGE(TAG, "ピアの登録に失敗しました");
        return;
    }

    led_init();
}

// データを送信する関数
void send_data_to_main(controller_data_t *data) {
    esp_err_t result = esp_now_send(PEER_MAC_ADDRESS, (uint8_t *) data, sizeof(controller_data_t));
    if (result == ESP_OK) {
        //ESP_LOGI(TAG, "送信指示完了");
    } else {
        ESP_LOGE(TAG, "送信エラー");
    }
}