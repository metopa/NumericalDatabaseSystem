/** @file numdb.h
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_NUMDB_H
#define NUMDB_NUMDB_H


#include "function_cache.h"
#include "event_counter.h"

#include "dummy_container.h"

#include "hash_table/fixed_hashtable_fair_lu.h"
#include "hash_table/fixed_hashtable_binary_heap.h"
#include "numdb/cndc/cndc.h"

#include "wst/weighted_search_tree.h"

#include "splay_tree/splay_tree_strategy.h"
#include "splay_tree/splay_tree_fair_lu.h"
#include "splay_tree/splay_tree_bottom_node.h"

#include "concurrent_adapters/binning_adapter.h"
#include "concurrent_adapters/coarse_lock_adapter.h"

#include "fair_lru.h"
#include "fair_lfu.h"

namespace numdb {
	template <typename ContainerTypeHolderT, size_t ThreadCount>
	using BinningConcurrentAdapter = containers::BinningConcurrentAdapterTypeHolder<ContainerTypeHolderT, ThreadCount>;

	template <typename ContainerTypeHolderT>
	using CoarseLockAdapter = containers::CoarseLockConcurrentAdapterTypeHolder<ContainerTypeHolderT>;

	using DummyContainer = containers::DummyContainerTypeHolder;

	template <typename HasherT = mmh2::MurmurHash2<void>>
	using LruHashtable = containers::FixedHashtableFairLeastUsedTypeHolder<utility::FairLRU, HasherT>;

	template <typename HasherT = mmh2::MurmurHash2<void>>
	using LfuHashtable = containers::FixedHashtableFairLeastUsedTypeHolder<utility::FairLFU, HasherT>;

	template <int DegradationRate, bool UseShortIndex = true,
			typename PriorityT = utility::WstPriority,
			typename HasherT = mmh2::MurmurHash2<void>>
	using PriorityHashtable = containers::FixedHashtableBinaryHeapTypeHolder<DegradationRate, PriorityT, HasherT, UseShortIndex>;

	template <bool UseBackoff = true, bool UseShortIndex = true,
			typename PriorityT = utility::WstPriority, typename HasherT = mmh2::MurmurHash2<void>>
	using CNDC = containers::CNDCTypeHolder<UseBackoff, UseShortIndex, PriorityT, HasherT>;

	template <typename SplayStrategyT = containers::CanonicalSplayStrategy, typename ComparatorT = std::less<>>
	using LruSplayTree = containers::SplayTreeFairLeastUsedTypeHolder<utility::FairLRU, SplayStrategyT, ComparatorT>;

	template <typename SplayStrategyT = containers::CanonicalSplayStrategy, typename ComparatorT = std::less<>>
	using LfuSplayTree = containers::SplayTreeFairLeastUsedTypeHolder<utility::FairLFU, SplayStrategyT, ComparatorT>;

	template <typename SplayStrategyT = containers::CanonicalSplayStrategy, typename ComparatorT = std::less<>>
	using BottomNodeSplayTree = containers::SplayTreeBottomNodeTypeHolder<SplayStrategyT, ComparatorT>;

	//TODO Add default value for Degradation Rate
	template <int DegradationRate, bool UseShortIndex = true, typename ComparatorT = std::less<>>
	using WeightedSearchTree = containers::WeightedSearchTreeTypeHolder<DegradationRate, UseShortIndex, ComparatorT>;
}

#endif //NUMDB_NUMDB_H
