/** @file fixed_hashtable_binary_heap.h
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_FIXED_HASHTABLE_BINARY_HEAP_H
#define NUMDB_FIXED_HASHTABLE_BINARY_HEAP_H

#include <murmurhash2/all.h>

#include "numdb/hash_table/fixed_hashtable_base.h"
#include "numdb/utils.h"
#include "numdb/priority.h"

namespace numdb {
	namespace containers {
		template <typename KeyT, typename ValueT, typename PriorityT,
				int DegradationRate, typename HasherT, bool UseShortIndex>
		class FixedHashtableBinaryHeap;

		template <typename KeyT, typename ValueT, typename PriorityT,
				int DegradationRate, typename HasherT, bool UseShortIndex>
		struct CacheContainerTraits<FixedHashtableBinaryHeap
				<KeyT, ValueT, PriorityT,
						DegradationRate, HasherT, UseShortIndex>> {
			using key_t = KeyT;
			using value_t = ValueT;
			using hasher_t = HasherT;
			using idx_t = std::conditional_t<UseShortIndex, uint32_t, size_t>;

			struct NodeBase {
				idx_t heap_idx = 0;
			};

			using node_base_t = NodeBase;
		};

		template <int DegradationRate, typename PriorityT = utility::WstPriority, typename HasherT = mmh2::MurmurHash2<void>, bool UseShortIndex = true>
		struct FixedHashtableBinaryHeapTypeHolder {
			template <typename KeyT, typename ValueT>
			using container_t = FixedHashtableBinaryHeap<KeyT, ValueT, PriorityT, DegradationRate, HasherT, UseShortIndex>;
		};

		template <typename KeyT, typename ValueT, typename PriorityT, int DegradationRate, typename HasherT, bool UseShortIndex>
		class FixedHashtableBinaryHeap : public FixedHashtableBase<
				FixedHashtableBinaryHeap<KeyT, ValueT, PriorityT, DegradationRate, HasherT, UseShortIndex>> {

		  public:
			using base_t = FixedHashtableBase<
					FixedHashtableBinaryHeap<KeyT, ValueT, PriorityT, DegradationRate, HasherT, UseShortIndex>>;
			using node_t = typename base_t::Node;
			using traits_t = CacheContainerTraits<FixedHashtableBinaryHeap<KeyT, ValueT, PriorityT, DegradationRate, HasherT, UseShortIndex>>;
			using idx_t = typename traits_t::idx_t;
			friend base_t;

			FixedHashtableBinaryHeap(size_t available_memory, double load_factor = 2) :
					base_t(available_memory, load_factor), heap_node_count_(0) {
				if (UseShortIndex && this->capacity() > std::numeric_limits<idx_t>::max() - 1)
					throw std::length_error(
							"Capacity (64-bit) exceeded max index value (32-bit). Disable short index optimization (UseShortIndex template param)");
				heap_nodes_.resize(this->capacity());
			}

		  protected:
			struct HeapNode {
				PriorityT priority;
				node_t* table_node;
			};

			void nodeAccessedImpl(node_t* node) {
				assert(node);
				idx_t heap_idx = node->heap_idx;
				assert(heap_idx < heap_node_count_);
				_(heap_idx).priority.access();
				topDownHeapify(heap_idx);
			}

			void nodeInsertedImpl(node_t* node, size_t priority) {
				assert(heap_node_count_ < heap_nodes_.size());
				idx_t heap_idx = heap_node_count_;
				heap_node_count_++;
				_(heap_idx).table_node = node;
				node->heap_idx = heap_idx;
				_(heap_idx).priority = PriorityT(priority);
				_(heap_idx).priority.access();
				bottomUpHeapify(heap_idx);
			}

			void nodeExtractedImpl(node_t* node) {
				assert(node);
				idx_t heap_idx = node->heap_idx;
				assert(heap_idx < heap_node_count_);
				heapRemove(heap_idx);
			}

			static size_t additionalNodeOverheadImpl() {
				return sizeof(HeapNode);
			}

			node_t* getLuNodeImpl() {
				idx_t evicted_node = heapRemove(0);
				return _(evicted_node).table_node;
			}

		  private:
			static constexpr idx_t null = static_cast<idx_t>(-1);

			void swapNodes(idx_t a, idx_t b) {
				std::swap(_(a), _(b));
				assert(_(a).table_node->heap_idx == b);
				assert(_(b).table_node->heap_idx == a);
				_(a).table_node->heap_idx = a;
				_(b).table_node->heap_idx = b;
			}

			idx_t heapRemove(idx_t node) {
				if (node != heap_node_count_ - 1) {
					swapNodes(node, (idx_t) (heap_node_count_ - 1));
					heap_node_count_--;
					if (heap_node_count_ > 0) {
						idx_t new_idx = topDownHeapify(node);
						if (new_idx == node)
							bottomUpHeapify(new_idx);
					}
				} else
					heap_node_count_--;
				return heap_node_count_;
			}

			idx_t topDownHeapify(idx_t idx) {
				idx_t max = heapLeft(idx);
				if (max == null)
					return idx;
				idx_t right = heapRight(idx);
				if (right != null && _(right).priority < _(max).priority)
					max = right;
				if (_(max).priority < _(idx).priority) {
					swapNodes(idx, max);
					if (DegradationRate > 0) {
						_(idx).priority.visit(DegradationRate);
						bottomUpHeapify(idx);
					}
					return topDownHeapify(max);
				} else
					return idx;
			}

			idx_t bottomUpHeapify(idx_t idx) {
				idx_t parent;
				while ((parent = heapParent(idx)) != null &&
					   _(idx).priority < _(parent).priority) {
					swapNodes(idx, parent);
					idx = parent;
				}
				return idx;
			}

			idx_t heapParent(idx_t i) {
				return i == 0 ? null : (i - 1) / 2;
			}

			idx_t heapLeft(idx_t i) {
				return i * 2 + 1 < heap_node_count_ ? i * 2 + 1 : null;
			}

			idx_t heapRight(idx_t i) {
				return i * 2 + 2 < heap_node_count_ ? i * 2 + 2 : null;
			}

			HeapNode& _(idx_t i) {
				assert(i < heap_node_count_);
				return heap_nodes_[i];
			}

			const HeapNode& _(idx_t i) const {
				assert(i < heap_node_count_);
				return heap_nodes_[i];
			}

			idx_t heap_node_count_;
			std::vector<HeapNode> heap_nodes_;
		};
	}
}
#endif //NUMDB_FIXED_HASHTABLE_BINARY_HEAP_H
