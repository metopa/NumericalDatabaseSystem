/** @file splay_tree_fair_lru.h
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_SPLAY_TREE_FAIR_LU_H
#define NUMDB_SPLAY_TREE_FAIR_LU_H

#include "numdb/splay_tree/splay_tree_base.h"
#include "numdb/fair_lru.h"
#include "numdb/utils.h"

namespace numdb {
	namespace containers {
		template <typename KeyT, typename ValueT,
				typename LuStrategyT,
				typename SplayStrategyT,
				typename ComparatorT>
		class SplayTreeFairLeastUsed;

		template <typename KeyT, typename ValueT,
				typename LuStrategyT,
				typename SplayStrategyT,
				typename ComparatorT>
		struct CacheContainerTraits<SplayTreeFairLeastUsed
				<KeyT, ValueT, LuStrategyT, SplayStrategyT, ComparatorT>> {
			using key_t = KeyT;
			using value_t = ValueT;
			using strategy_t = SplayStrategyT;
			using comparator_t = ComparatorT;
			using node_base_t = typename LuStrategyT::Node;
			static constexpr bool enableRefToSelf() { return true; }
		};

		template <
				typename LuStrategyT,
				typename SplayStrategyT,
				typename ComparatorT = std::less<>>
		struct SplayTreeFairLeastUsedTypeHolder {
			template <typename KeyT, typename ValueT>
			using container_t = SplayTreeFairLeastUsed<KeyT, ValueT,
					LuStrategyT, SplayStrategyT, ComparatorT>;
		};

		template <typename KeyT, typename ValueT,
				typename LuStrategyT,
				typename SplayStrategyT,
				typename ComparatorT>
		class SplayTreeFairLeastUsed : public SplayTreeBase<
				SplayTreeFairLeastUsed<KeyT, ValueT,
						LuStrategyT, SplayStrategyT, ComparatorT>> {
		  public:
			using base_t = SplayTreeBase<
					SplayTreeFairLeastUsed<KeyT, ValueT,
							LuStrategyT, SplayStrategyT, ComparatorT>>;
			using node_t = typename base_t::Node;

			SplayTreeFairLeastUsed(size_t max_node_count, ComparatorT comparator = {}) :
					base_t(max_node_count, std::move(comparator)) {}

			void nodeAccessedImpl(node_t* node) {
				lu_manager_.markRecentlyUsed(node);
			}
			void nodeVisitedImpl(node_t* node) {}
			void nodeInsertedImpl(node_t* node) {
				lu_manager_.insertNode(node);
			}
			void nodeExtractedImpl(node_t* node) {
				lu_manager_.extractNode(node);
			}
			node_t** getLuNodeRefImpl(const typename base_t::key_t& /*key*/) {
				node_t* node = static_cast<node_t*>(lu_manager_.extractLuNode());
				assert(node != nullptr);
				assert(node->hasRefToSelf());
				assert(node->getRefToSelf() != nullptr);
				return node->getRefToSelf();
			}

		  private:
			LuStrategyT lu_manager_;
		};
	}
}
#endif //NUMDB_SPLAY_TREE_FAIR_LU_H
