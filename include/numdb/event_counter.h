/** @file event_counter.h
 *  @brief This structure is used for measuring different metrics, like total count of retrieves, number of user function calls, etc.
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_EVENT_COUNTER_H
#define NUMDB_EVENT_COUNTER_H

#include <cstdint>
#include <ostream>
#include <atomic>

namespace numdb {
	namespace utility {
		struct EmptyEventCounter {
			void retrieve() {}
			void invokeUserFunc() {}
		};

		struct AtomicEventCounter {
			std::atomic<uint64_t> total_retrievals = {0};
			std::atomic<uint64_t> user_func_invocations = {0};

			void retrieve() {
				total_retrievals++;
			}

			void invokeUserFunc() {
				user_func_invocations++;
			}

			double cacheEfficiency() const {
				return 1 - static_cast<double>(user_func_invocations) / total_retrievals;
			}

			friend std::ostream& operator <<(std::ostream& out, const AtomicEventCounter& ec) {
				return out << "Total: " << ec.total_retrievals << std::endl
						   << "User func calls: " << ec.user_func_invocations << std::endl
						   << "Cache efficiency: " << ec.cacheEfficiency() << std::endl;
			}
		};
	}
}
#endif //NUMDB_EVENT_COUNTER_H
