#include <M5StickCPlus.h>
// IR
#include <IRremoteESP8266.h>
#include <IRsend.h>
// ADR Hat
#include <Wire.h>
#include "ADS1100.h"

// M5StickCのIR
const uint16_t kIrLed = 32;
IRsend irsend(kIrLed);
const uint32_t power_buttone_code = 0x7E8154AB;

// M5StickCのADC Hat
ADS1100 ads;
#define REF_VOL 3.3
#define ADC_BASE REF_VOL / 32768
const uint16_t default_max_data = 5; // 動作していない状態のデータ値の最大
const uint16_t default_min_data = 3; // 動作していない状態のデータ値の最小

void setup()
{
	M5.begin(true, true, true); // LCDEnable, PowerEnable, SerialEnable(115200)
	M5.Lcd.setRotation(1);
	M5.Lcd.fillScreen(BLACK);
	M5.Lcd.setCursor(15, 10); //文字表示の左上位置を設定

	irsend.begin();

	ads.getAddr_ADS1100(ADS1100_DEFAULT_ADDRESS); // 0x48, 1001 000 (ADDR = GND)
	ads.setGain(GAIN_ONE);						  // 1x gain(default)
	ads.setMode(MODE_CONTIN);					  // Continuous conversion mode (default)
	ads.setRate(RATE_128);						  // 128SPS
	ads.setOSMode(OSMODE_SINGLE);				  // Set to start a single-conversion
	ads.begin();
}

void loop()
{
	byte error;
	int8_t address;
	address = ads.ads_i2cAddress;
	Wire.beginTransmission(address);
	error = Wire.endTransmission();
	M5.update(); // ボタン押下の検知に必須

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
		voltage_view(ads_result);
		voltage_check();
	}
	else
	{
		// ADS Hatを検出できない場合
		Serial.println("ADS1100 Disconnected!");
		M5.Lcd.setTextFont(4);
		M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
		M5.Lcd.drawString("Not Found.", 0, 20, 2);
	}
	delay(10);
}

void send_power_toggle()
{
	Serial.println("send");
	// ここで赤外線信号を送信する
	irsend.sendNEC(power_buttone_code, 32);
}

void voltage_check()
{
}

void voltage_view(int16_t ads_result)
{
	int16_t vol;
	float temp;

	Serial.print("Digital Value of Analog Input between Channel 0 and 1: ");
	Serial.println(ads_result);

	char data[20] = {0};
	sprintf(data, "%d", ads_result);
	temp = ads_result * ADC_BASE * 4;
	vol = temp * 1000;

	M5.Lcd.fillRect(0, 120, M5.Lcd.width(), M5.Lcd.height() - 120, BLACK); // 描画範囲を消す
	M5.Lcd.setTextFont(2);

	M5.Lcd.setCursor(6, 120); //文字表示位置を設定
	M5.Lcd.setTextColor(ORANGE);
	M5.lcd.printf("Raw data : %s", data);

	M5.Lcd.setCursor(110, 120); //文字表示位置を設定
	M5.Lcd.setTextColor(GREEN);
	M5.lcd.printf("Convert to : %d", vol);
	M5.Lcd.setCursor(220, 120); //文字表示の左上位置を設定
	M5.lcd.print("mV");
}