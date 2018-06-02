#ifndef RepeatingAction_h_guard
#define RepeatingAction_h_guard

class RepeatingAction {
	private:
		unsigned long next_time;

	protected:
		void reset_clock() {
			next_time = 0;
		}

	public:
		void run_if_necessary(const unsigned long current_millis) {
			if(current_millis > next_time) {
				next_time = current_millis + run_action();
			}
		}

		// return delay before next action
		virtual unsigned long run_action() = 0;
};

#endif // RepeatingAction_h_guard
