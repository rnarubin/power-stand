#include "RepeatingAction.h"

#define LED_HOLD_MILLIS 300 // how long diagnostic LEDs should hold their blinks

class DiagnosticLed : public RepeatingAction {
	private:
		const int pin;
		uint8_t target_count = 0;
		uint8_t current_count;
		int led_state;

		void reset() {
			reset_clock();
			current_count = 0;
			digitalWrite(pin, led_state = LOW);
		}

	public:
		DiagnosticLed(const int pin)
		: pin(pin)
		{
			pinMode(pin, OUTPUT);
			reset();
		}

		void blink(const uint8_t times) {
			target_count = times;
			reset();
		}

		unsigned long run_action() {
			if(current_count == target_count) {
				current_count = 0;
				// pause to distinguish counting sequences
				return (LED_HOLD_MILLIS * 4);
			}
			else {
				if(led_state == LOW) {
					led_state = HIGH;
				}
				else {
					led_state = LOW;
					current_count++;
				}
				digitalWrite(pin, led_state);
				return LED_HOLD_MILLIS;
			}
		}
};

