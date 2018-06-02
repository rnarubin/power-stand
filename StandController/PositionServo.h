#include <Servo.h>

class PositionServo{
	private:
		Servo servo;
		uint8_t pos;

	public:
		uint8_t min_angle;
		uint8_t max_angle;

		PositionServo() {}

		void setup(const int pin, const uint8_t min_angle, const uint8_t max_angle){
			// for some reason the servo goes nuts if initialized before the main 'setup' method,
			// e.g. in the constructor. run this setup instead
			this->min_angle = min_angle;
			this->max_angle = max_angle;
			servo.attach(pin);
			recenter();
		}

		void move_to(const int16_t angle) {
			// min/max angle ultimately bound within uint8
			servo.write(pos = constrain(angle, min_angle, max_angle));
		}

		void rotate(const int16_t amount) {
			if(amount != 0){
				move_to(pos + amount);
				return true;
			}
			return false;
		}

		void recenter() {
			move_to(90);
		}
};
