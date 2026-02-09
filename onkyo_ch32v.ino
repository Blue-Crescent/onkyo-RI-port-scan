#include <Arduino.h>
// #include <TM1637.h>   ← もしTM1637ライブラリがCH32V003で動くならそのまま使用可

#define TX_PIN PD7       // 出力ピン（PD7など、実際のピン番号に注意）
// #define DIO 2
// #define CLK 4

#define MAX_PULSES 32

volatile uint8_t  pulseDurations[MAX_PULSES];
volatile uint8_t  pulseLevels[MAX_PULSES];
volatile uint8_t  pulseCount  = 0;
volatile uint8_t  pulseIndex  = 0;
volatile bool     sending     = false;

// TM1637 led(CLK, DIO);   // ← TM1637が使えるならコメント解除

// --------------------------------------------------
// TIM2 1ms周期割り込みハンドラ（WCHコアではこの名前で自動リンク）
// --------------------------------------------------
extern "C" void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
extern "C" void TIM2_IRQHandler(void)
{
    if (TIM2->INTFR & TIM_UIF)          // Update割り込みか？
    {
        TIM2->INTFR &= ~TIM_UIF;        // フラグクリア（必須）

        if (!sending) return;

        digitalWrite(TX_PIN, pulseLevels[pulseIndex]);
        if (--pulseDurations[pulseIndex] == 0)
        {
            pulseIndex++;
            if (pulseIndex >= pulseCount)
            {
                sending = false;
                // 必要ならここでTXをLowに戻すなど
                digitalWrite(TX_PIN, LOW);
            }
        }
    }
}

// --------------------------------------------------
// 1msタイマーの初期化（48MHzクロック前提）
// --------------------------------------------------
void setupTimer2_1ms(void)
{
    // クロック供給
    RCC->APB1PCENR |= RCC_APB1Periph_TIM2;

    // タイマーリセット
    TIM2->CTLR1 = 0;
    TIM2->ATRLR = 0;           // Auto-reload = 0 → 次に設定

    // 48MHz / 48 = 1MHzカウント（1μs分解能）
    TIM2->PSC = 23;            // Prescaler = 47 → 48MHz ÷ 48 = 1MHz

    // 1ms = 1000カウント
    TIM2->ATRLR = 999;         // Period = 999 (0〜999で1000カウント)

    TIM2->RPTCR  = 0;
    TIM2->SWEVGR = TIM_UG;     // Update Generation
    TIM2->INTFR  = 0;          // フラグ全部クリア

    // Update割り込み有効
    TIM2->DMAINTENR |= TIM_UIE;

    // NVIC有効化（優先度デフォルトでOKならこのままで）
    NVIC_EnableIRQ(TIM2_IRQn);

    // カウント開始
    TIM2->CTLR1 |= TIM_CEN;
}

// --------------------------------------------------
// 以下は元のコードとほぼ同じ（一部修正）
// --------------------------------------------------

void enqueuePulse(uint8_t level, uint16_t dur_ms)
{
    if (pulseCount >= MAX_PULSES) return;
    pulseLevels[pulseCount]   = (level == HIGH) ? HIGH : LOW;  // 非反転出力
    pulseDurations[pulseCount] = dur_ms;                       // msのまま（1ms割り込みで減らす）
    pulseCount++;
}

void prepareHeader() {
    enqueuePulse(HIGH, 3);
    enqueuePulse(LOW,  1);
}

void prepareData(uint16_t data) {
    for (int i = 11; i >= 0; i--) {
        enqueuePulse(HIGH, 1);
        bool bit = (data >> i) & 1;
        enqueuePulse(LOW, bit ? 2 : 1);
    }
}

void prepareTrailer() {
    enqueuePulse(HIGH, 1);
    enqueuePulse(LOW,  20);
}

void prepareAll(uint16_t data) {
    prepareHeader();
    prepareData(data);
    prepareTrailer();
}

void startSend() {
    pulseIndex = 0;
    sending    = true;
}

void sendCode(uint16_t data) {
    if (sending) return;
    pulseCount = 0;
    pulseIndex = 0;

    char text[8];
    sprintf(text, "%04X", data);
    // led.display(text);   // TM1637が使えるなら

    prepareAll(data);
    startSend();
}

void enterPermanentStandby(void) {
    __disable_irq();  // 割り込み禁止（これが重要）

    // 1. ペリフェラルクロック全部止める
    RCC->APB1PCENR = 0x00000000;
    RCC->APB2PCENR = 0x00000000;

    // 2. LSI確実にオフ
    RCC->CTLR &= ~RCC_LSION;

    // 3. 出力ピンをLOWにして入力に戻す（リーク電流最小化）
    digitalWrite(TX_PIN, LOW);
    pinMode(TX_PIN, INPUT);

    // 他の使ってるピン（CLK, DIOなど）も同様にLOW + INPUTにするとより良い
    // digitalWrite(CLK, LOW); pinMode(CLK, INPUT);
    // digitalWrite(DIO, LOW); pinMode(DIO, INPUT);

    // 4. Wakeup源を完全にゼロにする（EXTI全部無効）
    EXTI->INTENR = 0x00000000;
    EXTI->EVENR  = 0x00000000;
    EXTI->RTENR  = 0x00000000;
    EXTI->FTENR  = 0x00000000;

    // 5. Standbyモード選択
    PWR->CTLR |= PWR_CTLR_PDDS;  // PDDS = 1 でStandby

    // Deep Sleep有効（多くのArduinoコア例ではこれで十分。SCBは使わない）
    // SCB->SCR |= (1 << 2);  // SLEEPDEEP bit = 1 （ただしSCB未定義の場合省略可）

    // 6. ここでWFI（またはWFE）実行 → Standbyへ
    __WFI();   // Wait For Interrupt でStandbyに入る

    // ここには絶対に来ない
    while(1) {
        __WFI();  // 保険
    }
}

void setup()
{
    // 最初に1msタイマー開始
    setupTimer2_1ms();

    pinMode(TX_PIN, OUTPUT);
    digitalWrite(TX_PIN, LOW);

    // led.begin();   // TM1637があるなら
    // delay(1000);
    // digitalWrite(TX_PIN, HIGH);
    // delay(1000);
    // digitalWrite(TX_PIN, LOW);
    // delay(1000);

    // テスト送信
    //sendCode(0x170);   delay(1000);
    // delay(3000);
    // sendCode(0x7F);
    // delay(1000);

}

void loop()
{
    // digitalWrite(TX_PIN, HIGH);
    // delay(1000);
    // digitalWrite(TX_PIN, LOW);
    // delay(1000);
  if (!sending) {
    for (uint16_t code = 0x0; code <=10; code=code+0x1){
      sendCode(0x7F);
      delay(500);
    }
  }
  enterPermanentStandby();
}