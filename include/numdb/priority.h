/** @file priority.h
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_PRIORITY_H
#define NUMDB_PRIORITY_H

#include <cassert>
#include <cstddef>
#include <algorithm>

namespace numdb {
	namespace utility {
		class WstAvlPriority {
		  public:
			WstAvlPriority(unsigned int priority) : priority_(std::max<unsigned>(priority, 1)) {
				setAvlBalance(0);
			}

			void visit(int degradation_rate) {
				if (priority_ > 256 * degradation_rate)
					priority_ -= 256 * degradation_rate;
				else
					priority_ &= 0xFF;
			}

			void access() {
				uint32_t priority = priority_;
				constexpr uint32_t max_priority = (1 << 30) - 1;
				priority += (priority & 0xFF) << 8;
				priority_ = (std::min(priority, max_priority) & 0xFFFFFF00) +
							(priority_ & 0xFF);
			}

			size_t value() {
				return priority_;
			}

			int avlBalance() {
				return balance_;
			}

			void setAvlBalance(int b) {
				assert(b >= -1 && b <= 1);
				balance_ = b;
			}

			bool operator <(const WstAvlPriority& other) const {
				return priority_ < other.priority_;
			}

		  private:
			int32_t balance_ : 2;
			uint32_t priority_ : 30;
		};

		static_assert(sizeof(WstAvlPriority) == 4, "Invalid wst priority size");

		class WstPriority {
		  public:
			WstPriority(unsigned int priority = 0) :
					priority_(std::max<unsigned>(priority, 1)) {}

			void visit(int degradation_rate) {
				if (priority_ > 256 * degradation_rate)
					priority_ -= 256 * degradation_rate;
				else
					priority_ &= 0xFF;
			}

			void access() {
				uint64_t priority = priority_;
				priority += (priority & 0xFF) << 8;
				if (priority <= 0xFFFFFFFF)
					priority_ = (uint32_t)(priority & 0xFFFFFF00) +
							(priority_ & 0xFF);
				else
					priority_ = 0xFFFFFF00 + (priority_ & 0xFF);
			}

			size_t value() {
				return priority_;
			}

			bool operator <(const WstPriority& other) const {
				return priority_ < other.priority_;
			}

		  private:
			uint32_t priority_;
		};
	}
}

#endif //NUMDB_PRIORITY_H
