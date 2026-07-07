#include <string.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_now_handler.h"

#include "led_strip.h" // LEDライブラリを読み込む

#define LED_GPIO_PIN 38 // 通信状態を確認するためのLEDが接続されているピン番号
#define MAX_LEDS 1 // 制御するLEDの個数

static led_strip_handle_t led_strip; // LEDを操作するための識別情報
static int success_count = 100; // 通信の成功状態を表すスコア。初期値は満点の100とする
#define HISTORY_WEIGHT 100     // 過去の通信をどれだけの期間分反映させるかの基準値

static const char *TAG = "ESP_NOW_TX"; // ログ出力時に使用する識別名

// 受信側である本体の固有識別番号のMacアドレス
uint8_t PEER_MAC_ADDRESS[] = {0x90, 0x70, 0x69, 0x35, 0x38, 0xE8};

// 送信処理が完了したときに自動で呼び出される関数。正しく送れたかを確認して処理を行う
void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // 送信結果に応じて通信スコアを増減させ、簡易的な移動平均を計算する
    if (status == ESP_NOW_SEND_SUCCESS) {
        if (success_count < HISTORY_WEIGHT) success_count++; // 送信成功時はスコアを増やす
    } else {
        if (success_count > 0) success_count--; // 送信失敗時はスコアを減らす
    }

    // 通信成功率を0.0から1.0の範囲で算出する
    float ratio = (float)success_count / HISTORY_WEIGHT;

    // 成功率に応じてLEDの色を計算する。成功率が1.0に近いほど緑、0に近いほど赤になる
    uint32_t red   = (uint32_t)(255 * (1.0 - ratio)); // 成功率が低いほど赤色が強くなる
    uint32_t green = (uint32_t)(255 * ratio);       // 成功率が高いほど緑色が強くなる
    uint32_t blue  = 0;

    // 計算した色情報をLEDに設定して反映させる
    led_strip_set_pixel(led_strip, 0, red, green, blue);
    led_strip_refresh(led_strip);
}

// LEDの初期設定を行う関数
void led_init(void) {
    // LEDの接続ピンやカラーフォーマット、モデルなどの設定を行う
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO_PIN,
        .max_leds = MAX_LEDS,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, 
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    // 通信制御用の周辺機器の設定を行う
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, 
        .flags.with_dma = false,
    };
    // 設定を反映させてLEDデバイスを作成し、初期状態として一度消灯させる
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip); 
}

// 通信機能全体の初期化を行う関数
void wifi_now_init(void) {
    // データの保存領域であるフラッシュメモリの初期化を行う。Wi-Fi設定の保持に必要となる
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // ネットワーク基盤を初期化し、Wi-Fiを子機モードで起動する
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 機器間通信であるESP-NOWを初期化する
    ESP_ERROR_CHECK(esp_now_init());
    
    // データの送信が完了したときに動作するコールバック関数を登録する
    esp_now_register_send_cb(on_data_sent);

    // 送信相手となる本体側の情報を登録する準備を行う
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, PEER_MAC_ADDRESS, 6); // Macアドレスを設定する
    peerInfo.channel = 0; // 受信側と同じ無線チャンネルを使用する
    peerInfo.encrypt = false; // 暗号化は行わない

    // 登録用データをシステムに追加し、失敗した場合はログを出力して終了する
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        ESP_LOGE(TAG, "ピアの登録に失敗しました");
        return;
    }

    // ステータス表示用のLEDの初期化を行う
    led_init();
}

// 引数で受け取ったコントローラーの入力データを実際に送信する関数
void send_data_to_main(controller_data_t *data) {
    // 登録されたMacアドレスに向けてESP-NOW経由でデータを送信する
    esp_err_t result = esp_now_send(PEER_MAC_ADDRESS, (uint8_t *) data, sizeof(controller_data_t));
    if (result == ESP_OK) {
        // 送信指示が成功したときの処理
    } else {
        // 送信指示自体が失敗した場合はエラーログを出力する
        ESP_LOGE(TAG, "送信エラー");
    }
}
