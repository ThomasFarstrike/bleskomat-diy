#include "main.h"

#include <Badge2020_Buzzer.h>

unsigned int buttonDelay;

Badge2020_Buzzer buzzer;


// nokia
// float notes[ 13 ] = { 659.25,587.33,369.99,415.3 ,554.37,493.88,293.66,329.63,493.88,440,277.18,329.63,440 };

// pump it up
float notesPumpItUp[ 15 ] = { 392, 392, 392, 440, 440, 466.16, 0, 0, 392, 392, 392, 440, 440, 466.16, 0 };
float lengthsPumpItUp[ 15 ] = { 2, 2, 4, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2 };

/// bitcoin billionaire
float notesBB[ 19 ] = { 440.00,392.00,349.23,329.63,277.18, 293.66, 293.66, 349.23, 293.66 , 392.00, 0, 261.63, 261.63, 293.66, 293.66, 293.66, 293.66, 349.23, 293.66  };
float lengthsBB[ 19 ] = { 1,1,1,1,1, 3, 3, 1, 1, 12, 2, 1, 1, 1, 1, 1, 1, 3, 12 };

Badge2020_Buzzer::Badge2020_Buzzer() {
  ledcSetup( 5, 3000, 8 );
  setVolume( 0 );
  ledcAttachPin(BADGE2020_BUZZER, 0);
}

void Badge2020_Buzzer::setFrequency( int frequency ) {
  ledcWriteTone( 0, frequency );
}

void Badge2020_Buzzer::setVolume( int volume ) {
  ledcWrite( 0, volume );
}

void playSong(float notes[], float lengths[], int size, int pauseMs) {
	return;
	for (int i = 0; i < size; i++) {
		float freq = notes[i] * 2;
		if (freq > 0) {
			buzzer.setFrequency(freq);
		} else {
			buzzer.setVolume(0);
		}
		delay(90 * lengths[i]);
		buzzer.setVolume(0);
		delay(pauseMs);
	}
	buzzer.setVolume(0);
}

void playBitcoinBillionaire(){
	playSong(notesBB, lengthsBB, 19, 90);
}

void playPumpItUp() {
	playSong(notesPumpItUp, lengthsPumpItUp, 15, 30);
}

void setup() {
	Serial.begin(MONITOR_SPEED);
	spiffs::init();
	sdcard::init();
	config::init();
	logger::init();
	logger::write(firmwareName + ": Firmware version = " + firmwareVersion + ", commit hash = " + firmwareCommitHash);
	logger::write(config::getConfigurationsAsString());
	jsonRpc::init();
	screen::init();
	coinAcceptor::init();
	billAcceptor::init();
	button::init();
	buttonDelay = config::getUnsignedInt("buttonDelay");
}

void disinhibitAcceptors() {
	if (coinAcceptor::isInhibited()) {
		coinAcceptor::disinhibit();
	}
	if (billAcceptor::isInhibited()) {
		billAcceptor::disinhibit();
	}
}

void inhibitAcceptors() {
	if (!coinAcceptor::isInhibited()) {
		coinAcceptor::inhibit();
	}
	if (!billAcceptor::isInhibited()) {
		billAcceptor::inhibit();
	}
}

void resetAccumulatedValues() {
	coinAcceptor::resetAccumulatedValue();
	billAcceptor::resetAccumulatedValue();
}

float amountShown = 0;
unsigned long tradeCompleteTime = 0;

void writeTradeCompleteLog(const float &amount, const std::string &signedUrl) {
	std::string msg = "Trade completed:\n";
	msg += "  Amount  = " + util::floatToStringWithPrecision(amount, config::getUnsignedShort("fiatPrecision")) + " " + config::getString("fiatCurrency") + "\n";
	msg += "  URL     = " + signedUrl;
	logger::write(msg);
}

void runAppLoop() {
	coinAcceptor::loop();
	billAcceptor::loop();
	button::loop();
	const std::string currentScreen = screen::getCurrentScreen();
	if (currentScreen == "") {
		screen::showInsertFiatScreen(0);
		playPumpItUp();
	}
	float accumulatedValue = 0;
	accumulatedValue += coinAcceptor::getAccumulatedValue();
	accumulatedValue += billAcceptor::getAccumulatedValue();
	if (
		accumulatedValue > 0 &&
		currentScreen != "insertFiat" &&
		currentScreen != "tradeComplete"
	) {
		screen::showInsertFiatScreen(accumulatedValue);
		amountShown = accumulatedValue;
	}
	if (currentScreen == "insertFiat") {
		disinhibitAcceptors();
		if (button::isPressed()) {
			if (accumulatedValue > 0) {
				// Button pushed while insert fiat screen shown and accumulated value greater than 0.
				// Create a withdraw request and render it as a QR code.
				const std::string signedUrl = util::createSignedLnurlWithdraw(accumulatedValue);
				const std::string encoded = util::lnurlEncode(signedUrl);
				std::string qrcodeData = "";
				// Allows upper or lower case URI schema prefix via a configuration option.
				// Some wallet apps might not support uppercase URI prefixes.
				qrcodeData += config::getString("uriSchemaPrefix");
				// QR codes with only uppercase letters are less complex (easier to scan).
				qrcodeData += util::toUpperCase(encoded);
				screen::showTradeCompleteScreen(accumulatedValue, qrcodeData);
				writeTradeCompleteLog(accumulatedValue, signedUrl);
				inhibitAcceptors();
				playBitcoinBillionaire();
				tradeCompleteTime = millis();
			}
		} else {
			// Button not pressed.
			// Ensure that the amount shown is correct.
			if (amountShown != accumulatedValue) {
				screen::showInsertFiatScreen(accumulatedValue);
				amountShown = accumulatedValue;
				// play sound that gets higher with value
				float freq = 420 + accumulatedValue * 420;
				buzzer.setFrequency(freq);
				delay(20);
				buzzer.setVolume(0);
			}
		}
	} else if (currentScreen == "tradeComplete") {
		inhibitAcceptors();
		if (button::isPressed() && millis() - tradeCompleteTime > buttonDelay) {
			// Button pushed while showing the trade complete screen.
			// Reset accumulated values.
			resetAccumulatedValues();
			amountShown = 0;
			screen::showInsertFiatScreen(0);
			logger::write("Screen cleared");
			playPumpItUp();
		}
	}
}

void loop() {
	logger::loop();
	jsonRpc::loop();
	if (!jsonRpc::hasPinConflict() || !jsonRpc::inUse()) {
		runAppLoop();
	}
}
