#include "coin-acceptor/hx616.h"

namespace {

	enum class State {
		uninitialized,
		initialized,
		failed
	};
	State state = State::uninitialized;

	float valueIncrement = 1.00;
	uint16_t numPulses = 0;
	unsigned short coinInhibitPin;
	unsigned short coinSignalPin;

	void IRAM_ATTR onPinStateChange() {
		numPulses++;
	}
}

namespace coinAcceptor_hx616 {

	void init() {
		coinSignalPin = config::getUnsignedShort("coinSignalPin");
		coinInhibitPin = config::getUnsignedShort("coinInhibitPin");
		valueIncrement = config::getFloat("coinValueIncrement");
	}

	void loop() {
		if (state == State::uninitialized) {
			if (!(coinSignalPin > 0)) {
				logger::write("Cannot initialize coin acceptor: \"coinSignalPin\" not set", "warn");
				state = State::failed;
			} else if (!(coinInhibitPin > 0)) {
                                logger::write("Cannot initialize coin acceptor: \"coinInhibitPin\" not set", "warn");
                                state = State::failed;
			} else {
				logger::write("Initializing HX616 coin acceptor...");
				pinMode(coinSignalPin, INPUT_PULLUP);
				attachInterrupt(coinSignalPin, onPinStateChange, RISING);
				pinMode(coinInhibitPin, OUTPUT);
				state = State::initialized;
				coinAcceptor_hx616::disinhibit();
			}
		}
	}

	float getAccumulatedValue() {
		return numPulses * valueIncrement;
	}

	void resetAccumulatedValue() {
		numPulses = 0;
	}

	void inhibit() {
                if (state == State::initialized) {
                        digitalWrite(coinInhibitPin, LOW);
                }
        }

        void disinhibit() {
                if (state == State::initialized) {
                        digitalWrite(coinInhibitPin, HIGH);
                }
        }

}
