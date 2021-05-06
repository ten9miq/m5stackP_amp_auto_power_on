#include <M5StickCPlus.h>
// IR
#include <IRremoteESP8266.h>
#include <IRsend.h>
// ADR Hat
#include <Wire.h>
#include "ADS1100.h"

//TFT_eSPI liblary
TFT_eSprite Lcd_buff = TFT_eSprite(&M5.Lcd); // LCDのスプライト表示ライブラリ

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
const int16_t default_max_data = 10; // 動作していない状態のデータ値の最大
const int16_t default_min_data = -6; // 動作していない状態のデータ値の最小

// LED
#define LED_GPIO_NUM GPIO_NUM_10
#define LED_ON LOW
#define LED_OFF HIGH

void setup()
{
	M5.begin(true, true, true); // LCDEnable, PowerEnable, SerialEnable(115200)
	M5.Lcd.setRotation(1);
	M5.Axp.ScreenBreath(10);
	pinMode(LED_GPIO_NUM, OUTPUT); // 内蔵LED有効化

	//TFT_eSPI setup
	Lcd_buff.createSprite(m5.Lcd.width(), m5.Lcd.height());
	Lcd_buff.fillRect(0, 0, m5.Lcd.width(), m5.Lcd.height(), TFT_BLACK);
	Lcd_buff.pushSprite(0, 0);

	irsend.begin();

	ads.getAddr_ADS1100(ADS1100_DEFAULT_ADDRESS); // 0x48, 1001 000 (ADDR = GND)
	ads.setGain(GAIN_ONE);						  // 1x gain(default)
	ads.setMode(MODE_CONTIN);					  // Continuous conversion mode (default)
	ads.setRate(RATE_128);						  // 128SPS
	// ads.setRate(RATE_32);		  // 32SPS
	ads.setOSMode(OSMODE_SINGLE); // Set to start a single-conversion
	ads.begin();
}

void loop()
{
	byte error;
	int8_t address;
	address = ads.ads_i2cAddress;
	Wire.beginTransmission(address);
	error = Wire.endTransmission();
	M5.update();					// ボタン押下の検知に必須
	Lcd_buff.fillSprite(TFT_BLACK); // 画面を黒塗りでリセット

	// M5ボタン(BtnA)が押されたとき
	if (M5.BtnA.wasPressed())
	{
		send_power_toggle();
	}

	// ADS Hatによる電圧チェック処理
	if (error == 0)
	{
		int16_t ads_result;
		ads_result = ads.Measure_Differential();
		voltage_check(ads_result);
		voltage_view(ads_result);
	}
	else
	{
		// ADS Hatを検出できない場合
		Serial.println("ADS1100 Disconnected!");
		Lcd_buff.setTextFont(2);
		Lcd_buff.setCursor(10, 10); //文字表示位置を設定
		Lcd_buff.setTextColor(TFT_WHITE, TFT_BLACK);
		Lcd_buff.drawString("ADC Hat Not Found.", 0, 20, 2);
	}
	Lcd_buff.pushSprite(0, 0); // LCDに描画
	// LEDがついていれば消灯
	if (digitalRead(LED_GPIO_NUM) == LED_ON)
	{
		digitalWrite(LED_GPIO_NUM, LED_OFF);
	}
	delay(10);
}

void send_power_toggle()
{
	Serial.println("send");
	digitalWrite(LED_GPIO_NUM, LED_ON);
	// ここで赤外線信号を送信する
	irsend.sendNEC(power_buttone_code, 32);
}

void voltage_check(int16_t ads_result)
{
	if (ads_result < default_min_data || ads_result > default_max_data)
	{
		// M5.Beep.tone(4000);
		// delay(100);
		// M5.Beep.mute();
		Serial.print("default value over!!!!!!!!!! : ");
	}
}

void voltage_view(int16_t ads_result)
{
	int16_t vol;
	float temp;

	Serial.println(ads_result);

	char data[20] = {0};
	sprintf(data, "%d", ads_result);
	temp = ads_result * ADC_BASE * 4;
	vol = temp * 1000;

	Lcd_buff.setTextFont(2);

	Lcd_buff.setCursor(6, 120); //文字表示位置を設定
	Lcd_buff.setTextColor(TFT_ORANGE);
	Lcd_buff.printf("Raw data : %s", data);

	Lcd_buff.setCursor(110, 120); //文字表示位置を設定
	Lcd_buff.setTextColor(TFT_GREEN);
	Lcd_buff.printf("Convert to : %d", vol);
	Lcd_buff.setCursor(220, 120); //文字表示の左上位置を設定
	Lcd_buff.print("mV");
}