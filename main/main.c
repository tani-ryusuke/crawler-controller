#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_now_handler.h" // 自作した通信用のヘッダーファイルを読み込む

// レバーの上下左右が接続されているGPIOピンの番号を設定する
#define PIN_RIGHT 4  // 右方向のピン
#define PIN_LEFT  5  // 左方向のピン
#define PIN_UP    6  // 上方向のピン
#define PIN_DOWN  7  // 下方向のピン

// 8個のボタンがそれぞれ接続されているGPIOピンの番号を配列として定義する
static const int BTN_PINS[8] = {1, 2, 42, 41, 40, 39, 37, 36};

// レバーとボタンに使用するGPIOピンをまとめて初期化する関数
void gpio_init(void) {
    // レバーの4つのピンに対するビットマスクを作成する
    uint64_t mask = (1ULL<<PIN_RIGHT) | (1ULL<<PIN_LEFT) | (1ULL<<PIN_UP) | (1ULL<<PIN_DOWN);
    // ボタンの8つのピンに対するビットをループ処理でマスクに順次追加する
    for(int i=0; i<8; i++) {
        mask |= (1ULL << BTN_PINS[i]);
    }

    // 作成したマスクを用いてGPIOの設定構造体を準備する
    gpio_config_t io_conf = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_INPUT,            // すべて入力モードに設定する
        .pull_up_en = GPIO_PULLUP_ENABLE,   // ボタンを離したときに電位が安定するよう内部プルアップを有効にする
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    // 準備した設定を適用する
    gpio_config(&io_conf);
}

// レバーの傾きからテンキーの配置に対応した1から9の方向数値を計算する関数
int get_direction() {
    // スイッチが押されて回路が接地するとピンのレベルが0になるため0のときを真と判定する
    bool up    = gpio_get_level(PIN_UP) == 0;
    bool down  = gpio_get_level(PIN_DOWN) == 0;
    bool left  = gpio_get_level(PIN_LEFT) == 0;
    bool right = gpio_get_level(PIN_RIGHT) == 0;

    // 二つの方向が同時に押されている場合は斜め方向の数値を返す
    if (up && left)    return 7; // 左斜め前
    if (up && right)   return 9; // 右斜め前
    if (down && left)  return 1; // 左斜め後ろ
    if (down && right) return 3; // 右斜め後ろ
    
    // 一つの方向だけが押されている場合はその方向の数値を返す
    if (up)    return 8; // 前進
    if (down)  return 2; // 後進
    if (left)  return 4; // 左旋回
    if (right) return 6; // 右旋回
    
    // どこの方向にも傾いていない場合は中心を表す5を返す
    return 5;
}

// プログラム起動時に呼び出されるメイン関数
void app_main(void)
{
    // 自作した初期化関数を呼び出しESP-NOWによる通信の準備を行う
    wifi_now_init();

    // レバーとボタンの入力ピンを初期化する
    gpio_init();

    // 送信するためのデータを格納する構造体変数を宣言する
    controller_data_t send_data;
    
    // 入力の監視とデータ送信を絶え間なく繰り返す無限ループ
    while (1) {
        // レバーの入力状態を数値として取得し送信データに格納する
        send_data.dir = get_direction();

        // ループ処理により8個のボタンの入力状態を順番に取得して送信データに格納する
        for (int i = 0; i < 8; i++) {
            // ボタンが押されている場合は1を、離されている場合は0を格納する
            send_data.btn[i] = (gpio_get_level(BTN_PINS[i]) == 0);
        }
        
        // 構築した入力データをESP-NOWを通じて受信機に向けて送信する
        send_data_to_main(&send_data);

        // 20ミリ秒間処理を停止する。これにより1秒間に50回の頻度で送信ループが実行される
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}