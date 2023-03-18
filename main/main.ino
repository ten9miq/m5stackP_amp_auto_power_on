#include <M5StickCPlus.h>
// IR
#include <IRremoteESP8266.h>
#include <IRsend.h>
// ADR Hat
#include <Wire.h>

#include "ADS1100.h"

// TFT_eSPI liblary
TFT_eSprite Lcd_buff = TFT_eSprite(&M5.Lcd);  // LCDのスプライト表示ライブラリ

// M5StickCのIR
const uint16_t kIrLed_pin_number = 32;
IRsend irsend(kIrLed_pin_number);
const uint32_t power_buttone_code = 0x7E8154AB;

// M5StickCのADC Hat
ADS1100 ads;
#define REF_VOL 3.3
#define ADC_BASE REF_VOL / 32768
// 32sps
// const uint16_t default_max_data = 21; // 動作していない状態のデータ値の最大
// const uint16_t default_min_data = 14; // 動作していない状態のデータ値の最小
// 128sps
const int16_t default_max_data = 10;  // 動作していない状態のデータ値の最大
const int16_t default_min_data = -5;  // 動作していない状態のデータ値の最小

// LCD
const uint8_t max_ScreenBreath = 9;
const uint8_t min_ScreenBreath = 7;

// LED
#define LED_GPIO_NUM GPIO_NUM_10
#define LED_ON LOW
#define LED_OFF HIGH

// LCD Timer
const uint8_t timer_mm = 17;  // タイマーの分数
const uint8_t timer_ss = 56;  // タイマーの秒数
uint8_t mm = timer_mm, ss = timer_ss;
boolean is_measuring = true;
byte xsecs = 0, omm = 99, oss = 99;
uint32_t targetTime = 0;  // 次回更新のタイミング

void setup() {
	M5.begin(true, true, true);	 // LCDEnable, PowerEnable, SerialEnable(115200)
	M5.Lcd.setRotation(1);
	M5.Axp.ScreenBreath(max_ScreenBreath);
	pinMode(LED_GPIO_NUM, OUTPUT);	// 内蔵LED有効化

	// TFT_eSPI setup
	Lcd_buff.createSprite(m5.Lcd.width(), m5.Lcd.height());
	Lcd_buff.fillRect(0, 0, m5.Lcd.width(), m5.Lcd.height(), TFT_BLACK);
	Lcd_buff.pushSprite(0, 0);

	irsend.begin();

	ads.getAddr_ADS1100(ADS1100_DEFAULT_ADDRESS);  // 0x48, 1001 000 (ADDR = GND)
	ads.setGain(GAIN_ONE);						   // 1x gain(default)
	ads.setMode(MODE_CONTIN);					   // Continuous conversion mode (default)
	ads.setRate(RATE_128);						   // 128SPS
	// ads.setRate(RATE_32);		  // 32SPS
	ads.setOSMode(OSMODE_SINGLE);  // Set to start a single-conversion
	ads.begin();

	// timer
	targetTime = millis() + 1000;
}

void loop() {
	byte error;
	int8_t address;
	address = ads.ads_i2cAddress;
	Wire.beginTransmission(address);
	error = Wire.endTransmission();
	M5.update();					 // ボタン押下の検知に必須
	Lcd_buff.fillSprite(TFT_BLACK);	 // 画面を黒塗りでリセット

	// M5ボタン(BtnA)が押されたとき
	if (M5.BtnA.wasReleased()) {
		send_power_toggle();
	}
	if (M5.BtnB.wasReleased()) {
		timer_reset();
	}
	if (M5.Axp.GetBtnPress() == 2) {
		esp_restart();
	}

	lcd_timer_view();

	// ADS Hatによる電圧チェック処理
	if (error == 0) {
		int16_t ads_result;
		ads_result = ads.Measure_Differential();
		voltage_check(ads_result);
		voltage_view(ads_result);
	} else {
		// ADS Hatを検出できない場合
		Serial.println("ADS1100 Disconnected!");
		Lcd_buff.setTextFont(2);
		Lcd_buff.setCursor(10, 10);	 // 文字表示位置を設定
		Lcd_buff.setTextColor(TFT_WHITE, TFT_BLACK);
		Lcd_buff.drawString("ADC Hat Not Found.", 0, 20, 2);
	}
	Lcd_buff.pushSprite(0, 0);	// LCDに描画
	delay(10);

	// LEDがついていれば消灯
	if (digitalRead(LED_GPIO_NUM) == LED_ON) {
		digitalWrite(LED_GPIO_NUM, LED_OFF);
	}
}

void timer_reset() {
	mm = timer_mm;
	ss = timer_ss;
	is_measuring = true;
}

void lcd_timer_view() {
	if (targetTime < millis() && is_measuring) {
		// Set next update for 1 second later
		targetTime = millis() + 1000;
		// Adjust the time values by adding 1 second
		if (ss == 0 && mm == 0) {
			is_measuring = false;  // stop
		} else {
			ss = ss - 1;
		}
		if (ss == 255) {	  // Check for roll-over
			ss = 59;		  // Reset seconds to zero
			mm = mm - 1;	  // Advance minute
			if (mm == 255) {  // Check for roll-over
				mm = 0;
			}
		}
	}

	// タイマーが停止したらバックライトを消す
	if (!is_measuring) {
		M5.Axp.ScreenBreath(min_ScreenBreath);
	} else {
		M5.Axp.ScreenBreath(max_ScreenBreath);
	}
	// Draw digital time
	int xpos = -3;
	int ypos = 10;	// Top left corner ot clock text, about half way down
	int ysecs = ypos;
	Lcd_buff.setTextColor(TFT_WHITE);
	// Redraw hours and minutes time every minute
	omm = mm;
	if (mm < 10) xpos += Lcd_buff.drawChar('0', xpos, ypos, 8);
	xpos += Lcd_buff.drawNumber(mm, xpos, ypos, 8);
	xsecs = xpos;
	// Redraw seconds time every second
	oss = ss;
	xpos = xsecs;
	if (ss % 2) {											  // Flash the colons on/off
		Lcd_buff.setTextColor(0x39C4, TFT_BLACK);			  // Set colour to grey to dim colon
		xpos += Lcd_buff.drawChar(':', xsecs, ysecs - 8, 8);  // Seconds colon
		Lcd_buff.setTextColor(TFT_WHITE);
	} else {
		xpos += Lcd_buff.drawChar(':', xsecs, ysecs - 8, 8);  // Seconds colon
	}
	// Draw seconds
	if (ss < 10) xpos += Lcd_buff.drawChar('0', xpos, ysecs, 8);  // Add leading zero
	Lcd_buff.drawNumber(ss, xpos, ysecs, 8);					  // Draw seconds
}

void send_power_toggle() {
	Serial.println("send");
	digitalWrite(LED_GPIO_NUM, LED_ON);
	// ここで赤外線信号を送信する
	irsend.sendNEC(power_buttone_code, 32);
}

void voltage_check(int16_t ads_result) {
	if (ads_result < default_min_data || ads_result > default_max_data) {
		Serial.print("default value over!!!!!!!!!! : ");
		Lcd_buff.setTextFont(2);
		Lcd_buff.setCursor(6, 100);	 // 文字表示位置を設定
		Lcd_buff.setTextColor(TFT_RED);
		Lcd_buff.printf("Default range over(%d<%d) : %d", default_min_data, default_max_data, ads_result);
		if (!is_measuring) {
			send_power_toggle();
		}
		timer_reset();
	}
}

void voltage_view(int16_t ads_result) {
	int16_t vol;
	float temp;

	Serial.println(ads_result);

	char data[20] = {0};
	sprintf(data, "%d", ads_result);
	temp = ads_result * ADC_BASE * 4;
	vol = temp * 1000;

	Lcd_buff.setTextFont(2);

	Lcd_buff.setCursor(6, 120);	 // 文字表示位置を設定
	Lcd_buff.setTextColor(TFT_ORANGE);
	Lcd_buff.printf("Raw data : %s", data);

	Lcd_buff.setCursor(110, 120);  // 文字表示位置を設定
	Lcd_buff.setTextColor(TFT_GREEN);
	Lcd_buff.printf("Convert to : %d", vol);
	Lcd_buff.setCursor(220, 120);  // 文字表示の左上位置を設定
	Lcd_buff.print("mV");
}