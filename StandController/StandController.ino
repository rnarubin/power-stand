#include "PositionServo.h"
#include "DiagnosticLed.h"
#include "RepeatingAction.h"
#include <EEPROM.h>

#define CLICK_PIN 2
#define X_PIN 1
#define Y_PIN 3

const char* NOT_OK = "NOT_OK";
const char* OK = NOT_OK+4;

uint8_t dead_zone; // joystick deadzone to prevent jitter at rest. hardware dependent

class AnalogStickReader : public RepeatingAction {
	private:
		PositionServo* servo;
		const int input_pin;
		int8_t scale; // signed to enable reversing a stick's direction at runtime
		uint8_t update_delay;

	public:
		AnalogStickReader(PositionServo* servo, const int input_pin) 
		: servo(servo), input_pin(input_pin)
		{}

		void setup(const int8_t scale, const uint8_t update_delay) {
			this->scale = scale;
			this->update_delay = update_delay;
		}

		unsigned long run_action() {
			int relative_val = map(analogRead(input_pin), 0, 1023, -128, 127);

			if(abs(relative_val) > static_cast<int>(dead_zone)) {
				servo->rotate(relative_val/scale);
			}

			return update_delay;
		}
};

// implicitly populated addresses of variables stored in EEPROM
enum class EepromAddress : int {
	// the reason pins are configurable is to swap at runtime if the peripherals are plugged wrong
	pan_output_pin,
	pan_min_angle,
	pan_max_angle,
	tilt_output_pin,
	tilt_min_angle,
	tilt_max_angle,
	x_scale,
	x_update_delay,
	y_scale,
	y_update_delay,
	stick_dead_zone,
	ENUM_LENGTH
};

enum class ErrorCode : uint8_t {
	none,
	msg_buffer_exceeded
};

DiagnosticLed onboard_led(LED_BUILTIN);
PositionServo pan;
PositionServo tilt; 
AnalogStickReader x_stick(&pan, X_PIN);
AnalogStickReader y_stick(&tilt, Y_PIN);

void setup() {
	//populate_eeprom();
	Serial.begin(9600);
	pinMode(CLICK_PIN, INPUT);
	digitalWrite(CLICK_PIN, HIGH);

	dead_zone = read_eeprom(EepromAddress::stick_dead_zone);

	pan.setup(
		read_eeprom(EepromAddress::pan_output_pin),
		read_eeprom(EepromAddress::pan_min_angle),
		read_eeprom(EepromAddress::pan_max_angle)
	);

	tilt.setup(
		read_eeprom(EepromAddress::tilt_output_pin),
		read_eeprom(EepromAddress::tilt_min_angle),
		read_eeprom(EepromAddress::tilt_max_angle)
	);

	x_stick.setup(
		read_eeprom(EepromAddress::x_scale),
		read_eeprom(EepromAddress::x_update_delay)
	);

	y_stick.setup(
		read_eeprom(EepromAddress::y_scale),
		read_eeprom(EepromAddress::y_update_delay)
	);
}

void loop() {
	const unsigned long current_millis = millis();

	read_serial();
	
	if(stick_pressed()){
		pan.recenter();
		tilt.recenter();
	}
	else{
		x_stick.run_if_necessary(current_millis);
		y_stick.run_if_necessary(current_millis);
	}

	onboard_led.run_if_necessary(current_millis);
}

void report_error(const ErrorCode error) {
	onboard_led.blink(static_cast<uint8_t>(error));
}

// woah boy, function pointer syntax is fun
const char* (*get_message_handler(char start, bool (**is_message_end_out)(char)))(char*, uint8_t) {
	// include minus signs for negative numbers
	static auto is_not_digit = [](char c) { return !(isDigit(c) || c == '-'); };
	static auto is_newline = [](char c) { return c == '\n'; };
	static auto any_character = [](char c) { return true; };

	static auto servo_action = [](PositionServo* servo, void (PositionServo::*action)(int16_t),
		char* msg, uint8_t len) -> const char*{
		msg[len] = '\0';
		(servo->*action)(atoi(msg));
		return OK;
	};

	switch(start) {
		case 'e':
			static auto echo = [](char* msg, uint8_t len) -> const char*{
				msg[len] = '\0';
				return msg;
			};
			*is_message_end_out = is_newline;
			return echo;

		case 'b':
			static auto blink = [&](char* msg, uint8_t len)  {
				msg[len] = '\0';
				onboard_led.blink(atoi(msg));
				return OK;
			};
			*is_message_end_out = is_not_digit;
			return blink;

		case 'p': 
			static auto pan_move_to = [&](char* msg, uint8_t len) {
				return servo_action(&pan, &PositionServo::move_to, msg, len);
			};
			*is_message_end_out = is_not_digit;
			return pan_move_to;

		case 't':
			static auto tilt_move_to = [&](char* msg, uint8_t len) {
				return servo_action(&tilt, &PositionServo::move_to, msg, len);
			};
			*is_message_end_out = is_not_digit;
			return tilt_move_to;

		case 'x': // pan relative
			static auto pan_rotate = [&](char* msg, uint8_t len) {
				return servo_action(&pan, &PositionServo::rotate, msg, len);
			};
			*is_message_end_out = is_not_digit;
			return pan_rotate;

		case 'y': // tilt relative
			static auto tilt_rotate = [&](char* msg, uint8_t len) {
				return servo_action(&tilt, &PositionServo::rotate, msg, len);
			};
			*is_message_end_out = is_not_digit;
			return tilt_rotate;

		case 's':
			static auto eeprom_store = [](char* msg, uint8_t len) {
				msg[len] = '\0';
				for(uint8_t i = 0; i < (len-1); i++){
					if(msg[i] == ':') {
						msg[i] = '\0';
						set_variable(atoi(msg), atoi(msg+i+1));
						return OK;
					}
				}
				// value delimiter not found
				return NOT_OK;
			};
			*is_message_end_out = is_newline;
			return eeprom_store;

		case 'r':
			static auto eeprom_reset = [](char* msg, uint8_t len) {
				populate_eeprom();
				return OK;
			};
			*is_message_end_out = any_character;
			return eeprom_reset;

		case 'd':
			static auto eeprom_dump = [](char* msg, uint8_t len) -> const char* {
				static char buf[static_cast<int>(EepromAddress::ENUM_LENGTH)*2+1];
				char* ptr = buf;
				for(int addr = 0; addr < static_cast<int>(EepromAddress::ENUM_LENGTH); addr++) {
					uint8_t val = EEPROM.read(addr);
					itoa(val, ptr, 16);
					ptr += 2;
				}
				return buf;
			};
			*is_message_end_out = any_character;
			return eeprom_dump;

		default:
			// unknown message character
			return NULL;
	}
}

void read_serial() {
	#define BUF_LEN 256
	static char buf[BUF_LEN];
	static uint8_t pos = 0; // current position in buffer
	static bool (*is_message_end)(char) = NULL;
	static const char* (*message_handler)(char*, uint8_t) = NULL;

	if(Serial.available()){
		char c = Serial.read();
		if(is_message_end == NULL) {
			check_start:
			// not currently populating a message.
			// check if this character starts one
			message_handler = get_message_handler(c, &is_message_end);
		}
		else if(is_message_end(c)) {
			const char* response = message_handler(buf, pos);
			if(response != NULL) {
				Serial.print(response);
			}
			pos = 0;
			is_message_end = NULL;
			message_handler = NULL;

			// non-matched character could start a new message
			goto check_start;
		}
		else if(pos >= (BUF_LEN - 1)){
			// allow 1 extra space to null terminate the buffer
			report_error(ErrorCode::msg_buffer_exceeded);
			pos = 0;
		}
		else {
			buf[pos++] = c;
		}
	}
	#undef BUF_LEN
}

bool stick_pressed() {
	return !digitalRead(CLICK_PIN);
}

uint8_t read_eeprom(EepromAddress address) {
	return EEPROM.read(static_cast<int>(address));
}

void write_eeprom(EepromAddress address, uint8_t val) {
	EEPROM.write(static_cast<int>(address), val);
}

void set_variable(int address, uint8_t val) {

	switch(static_cast<EepromAddress>(address)) {
		case EepromAddress::pan_output_pin:
			break;

		default:
			// TODO error code
			// don't write to eeprom, just return
			return;
	}

	EEPROM.write(address, val);
}

// if values have not been written to EEPROM, use these
#define PAN_PIN 5
#define PAN_MIN_ANGLE 10 // restrict pan angle because these servos are crap and can't do 180
#define PAN_MAX_ANGLE 170
#define TILT_PIN 6
#define TILT_MIN_ANGLE 40 // restrict tilt angle to limit weight strain at extremes 
#define TILT_MAX_ANGLE 140
#define SCALE 40
#define UPDATE_DELAY 30
#define DEAD_ZONE 20

void populate_eeprom() {
	write_eeprom(EepromAddress::pan_output_pin, PAN_PIN);
	write_eeprom(EepromAddress::pan_min_angle, PAN_MIN_ANGLE);
	write_eeprom(EepromAddress::pan_max_angle, PAN_MAX_ANGLE);
	write_eeprom(EepromAddress::tilt_output_pin, TILT_PIN);
	write_eeprom(EepromAddress::tilt_min_angle, TILT_MIN_ANGLE);
	write_eeprom(EepromAddress::tilt_max_angle, TILT_MAX_ANGLE);
	write_eeprom(EepromAddress::x_scale, SCALE);
	write_eeprom(EepromAddress::x_update_delay, UPDATE_DELAY);
	write_eeprom(EepromAddress::y_scale, SCALE);
	write_eeprom(EepromAddress::y_update_delay, UPDATE_DELAY);
	write_eeprom(EepromAddress::stick_dead_zone, DEAD_ZONE);
}

