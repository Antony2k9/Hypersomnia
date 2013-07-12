#pragma once
#include "timer.h"
#include <functional>
#include <vector>

namespace augmentations {
	namespace util {
		class animator {
			timer tm;
			std::function<void (float)> callback;
			float init_val, diff, miliseconds;
		public:
			struct method {
				std::function<float (float)> increasing_func;
				float left_x, diff_x, left_y, diff_y;

				method(float left_x, float right_x, const std::function<float (float)>& increasing_func); 
				
				static float linear(float);
				static float quadratic(float);
				static float sinusoidal(float);
				static float hyperbolic(float);
				static float logarithmic(float);
				static float exponential(float);
			} how;
			static method LINEAR, QUADRATIC, SINUSOIDAL, HYPERBOLIC, LOGARITHMIC, EXPONENTIAL;
			animator(const std::function<void (float)>& callback, float init_val, float desired_val, float miliseconds, method how);
			
			void start();
			bool animate();
		};

		struct animation {
			std::vector<animator*> animators;

			int loops, current_loop;
			unsigned current;
			animation(int loops = -1);

			void start();
			bool animate();
		};
	}
}

