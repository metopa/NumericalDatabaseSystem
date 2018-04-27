/** @file splay_tree_strategy.h
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_SPLAY_TREE_STRATEGY_H
#define NUMDB_SPLAY_TREE_STRATEGY_H

#include <ostream>

namespace numdb {
	namespace containers {
		struct CanonicalSplayStrategy {
			bool shouldSplay(CanonicalSplayStrategy* child) {
				return true;
			}

			CanonicalSplayStrategy(uint64_t score) {}

			void visited() {}
			void accessed() {}
			void dumpStrategy(std::ostream& out) const {}
		};

		struct AccessCountSplayStrategy {
			uint32_t accesses = 0;

			AccessCountSplayStrategy(uint64_t score) : accesses((uint32_t) score) {}

			bool shouldSplay(AccessCountSplayStrategy* child) {
				return accesses < child->accesses;
			}
			void visited() {}
			void accessed() {
				accesses = std::max(accesses + 1, std::numeric_limits<uint32_t>::max());
			}

			void dumpStrategy(std::ostream& out) const {
				out << "<accesses: " << accesses << ">";
			}
		};

		template <unsigned char SCORE_BOOST,
				unsigned char SCORE_DEGRADATION,
				unsigned char MAX_SCORE>
		struct ParametrizedAccessCountSplayStrategy {
			unsigned char score;

			ParametrizedAccessCountSplayStrategy(uint64_t score) :
					score((unsigned char) score) {}

			bool shouldSplay(ParametrizedAccessCountSplayStrategy* child) {
				return score < child->score;
			}
			void visited() {
				score = std::max<unsigned char>(score - SCORE_DEGRADATION, 0);
			}
			void accessed() {
				score = std::min<unsigned char>(score + SCORE_BOOST, MAX_SCORE);
			}

			void dumpStrategy(std::ostream& out) const {
				out << "<score: " << score << '/' << MAX_SCORE << ">";
			}
		};

		template <unsigned DEGRADATION_RATE>
		class WstSplayStrategy {
		  public:
			WstSplayStrategy(unsigned int priority) :
					priority_(std::max<unsigned>(priority, 1)) {}

			void visited() {
				if (priority_ > 256 * DEGRADATION_RATE)
					priority_ -= 256 * DEGRADATION_RATE;
				else
					priority_ &= 0xFF;
			}

			void accessed() {
				uint32_t priority = priority_;
				constexpr uint32_t max_priority = (const uint32_t) -1;
				priority += (priority_ & 0xFF) << 8;
				if (priority_ < priority)
					priority_ = priority & 0xFFFFFF00 + priority_ & 0xFF;
				else
					priority_ = max_priority & 0xFFFFFF00 + priority_ & 0xFF;
			}

			bool shouldSplay(WstSplayStrategy* child) const {
				return priority_ < child->priority_;
			}

			void dumpStrategy(std::ostream& out) const {
				out << "<score: " << (priority_ & ~0xFF)
					<< '/' << (priority_ & 0xFF) << ">";
			}

		  private:
			uint32_t priority_;
		};
	}
}
#endif //NUMDB_SPLAY_TREE_STRATEGY_H
