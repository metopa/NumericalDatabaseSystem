/** @file fixed_hashtable_fair_lu.h
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_FIXED_HASHTABLE_FAIR_LU_H
#define NUMDB_FIXED_HASHTABLE_FAIR_LU_H

#include <murmurhash2/all.h>

#include "numdb/hash_table/fixed_hashtable_base.h"
#include "numdb/utils.h"

namespace numdb {
	namespace containers {
		template <typename KeyT, typename ValueT, typename LuStrategyT, typename HasherT>
		class FixedHashtableFairLeastUsed;

		template <typename KeyT, typename ValueT, typename LuStrategyT, typename HasherT>
		struct CacheContainerTraits<FixedHashtableFairLeastUsed<KeyT, ValueT, LuStrategyT, HasherT>> {
			using key_t = KeyT;
			using value_t = ValueT;
			using lu_strategy_t = LuStrategyT;
			using hasher_t = HasherT;
			using node_base_t = typename LuStrategyT::Node;
		};

		template <typename LuStrategyT, typename HasherT = mmh2::MurmurHash2<void>>
		struct FixedHashtableFairLeastUsedTypeHolder {
			template <typename KeyT, typename ValueT>
			using container_t = FixedHashtableFairLeastUsed<KeyT, ValueT, LuStrategyT, HasherT>;
		};

		template <typename KeyT, typename ValueT, typename LuStrategyT, typename HasherT>
		class FixedHashtableFairLeastUsed :
				public FixedHashtableBase<FixedHashtableFairLeastUsed<KeyT, ValueT, LuStrategyT, HasherT>> {

		  public:
			using base_t = FixedHashtableBase<
					FixedHashtableFairLeastUsed<KeyT, ValueT, LuStrategyT, HasherT>>;
			using node_t = typename base_t::Node;

			friend base_t;

			FixedHashtableFairLeastUsed(size_t available_memory, double load_factor = 2) :
					base_t(available_memory, load_factor) {}

		  protected:
			void nodeAccessedImpl(node_t* node) {
				lu_strategy_.markRecentlyUsed(node);
			}

			void nodeInsertedImpl(node_t* node, size_t priority) {
				lu_strategy_.insertNode(node);
			}

			void nodeExtractedImpl(node_t* node) {
				lu_strategy_.extractNode(node);
			}

			node_t* getLuNodeImpl() {
				return static_cast<node_t*>(lu_strategy_.extractLuNode());
			}

			static size_t additionalNodeOverheadImpl() {
				return 0;
			}

		  private:
			LuStrategyT lu_strategy_;
		};
	}
}

#endif //NUMDB_FIXED_HASHTABLE_FAIR_LU_H
