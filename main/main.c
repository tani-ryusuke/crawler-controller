#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_now_handler.h" // 自作したヘッダーを読み込む

// --- GPIO設定（レバー） ---
#define PIN_RIGHT 4
#define PIN_LEFT  5
#define PIN_UP    6
#define PIN_DOWN  7

// ボタン8個（J3列：38番LEDを飛ばした配置）
static const int BTN_PINS[8] = {1, 2, 42, 41, 40, 39, 37, 36};

// GPIOの初期化
void gpio_init(void) {
    // レバーとボタンの全ピンをマスク
    uint64_t mask = (1ULL<<PIN_RIGHT) | (1ULL<<PIN_LEFT) | (1ULL<<PIN_UP) | (1ULL<<PIN_DOWN);
    for(int i=0; i<8; i++) {
        mask |= (1ULL << BTN_PINS[i]);
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,   // プルアップ有効
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

// テンキー方向を計算する関数（1~9)
int get_direction() {
    bool up    = gpio_get_level(PIN_UP) == 0;
    bool down  = gpio_get_level(PIN_DOWN) == 0;
    bool left  = gpio_get_level(PIN_LEFT) == 0;
    bool right = gpio_get_level(PIN_RIGHT) == 0;

    if (up && left)    return 7;
    if (up && right)   return 9;
    if (down && left)  return 1;
    if (down && right) return 3;
    if (up)    return 8;
    if (down)  return 2;
    if (left)  return 4;
    if (right) return 6;
    return 5;
}

void app_main(void)
{
    // ESP-NOWの初期化（自作関数を呼ぶ）
    wifi_now_init();

    gpio_init();

    // 送信するデータの準備
    controller_data_t send_data;
    
    while (1) {
        //レバーの状態（テンキー方向）を取得
        send_data.dir = get_direction();

        //ボタン8個の状態を取得
        for (int i = 0; i < 8; i++) {
            send_data.btn[i] = (gpio_get_level(BTN_PINS[i]) == 0);
        }
        //データを送信
        send_data_to_main(&send_data);

        // 1秒に50回送信する
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}