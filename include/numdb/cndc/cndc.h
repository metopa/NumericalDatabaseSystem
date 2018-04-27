/** @file cndc.h
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_CNDC_H
#define NUMDB_CNDC_H

#include <cassert>
#include <cmath>
#include <mutex>
#include <vector>
#include <atomic>
#include <ostream>
#include <memory>
#include <experimental/optional>

#include <murmurhash2/all.h>

#include "numdb/utils.h"
#include "numdb/priority.h"

namespace numdb {
	namespace containers {
		template <typename KeyT, typename ValueT,
				typename PriorityT, typename HasherT,
				bool UseBackoff, bool UseShortIndex>
		class CNDC {
		  public:
			using key_t = KeyT;
			using value_t = ValueT;
			using hasher_t = HasherT;
			using priority_t = PriorityT;
			using optional_value_t = std::experimental::optional<value_t>;
			using idx_t = std::conditional_t<UseShortIndex, uint32_t, size_t>;
			using lock_guard_t = std::unique_lock<std::mutex>;
			using backoff_t = utility::ExpBackoff;

		  private:
			struct HashTableNode {
				void extract(HashTableNode** this_node_ref) {
					assert(this_node_ref != nullptr);
					assert(this == *this_node_ref);
					*this_node_ref = next_;
				}

				void insertAfter(HashTableNode** prev_node_ref) {
					assert(prev_node_ref != nullptr);
					next_ = *prev_node_ref;
					*prev_node_ref = this;
				}

				HashTableNode* next_;
				std::atomic<idx_t> heap_node_;
				std::atomic<uint32_t> version_;
				key_t key_;
				value_t value_;
			};

			struct HeapNode {
				HashTableNode* ht_node_ = nullptr;
				priority_t priority_;

				bool operator <(const HeapNode& other) const {
					return priority_ < other.priority_;
				}
			};

			struct LockedHeapNode {
				idx_t idx;
				lock_guard_t lg;
			};


		  public:
			static constexpr size_t maxElemCountForCapacity(
					size_t capacity, double load_factor) {
				return static_cast<size_t>(
						capacity / (sizeof(HashTableNode) + sizeof(HashTableNode*) / load_factor));
			}

			static constexpr size_t elementSize(double load_factor) {
				return sizeof(HashTableNode) + sizeof(HeapNode) + sizeof(std::mutex) +
					   (size_t) std::ceil((sizeof(HashTableNode*) + sizeof(std::mutex)) / load_factor);
			}

			static constexpr bool isThreadsafe() {
				return true;
			}

			CNDC(size_t available_memory, double load_factor = 2) :
					CNDC(maxElemCountForCapacity(available_memory, load_factor), load_factor, 0) {}

		  private:
			CNDC(size_t element_count, double load_factor, char dummy) :
					max_count_(element_count), load_factor_(load_factor), count_(0),
					buckets_(static_cast<size_t>(element_count / load_factor), nullptr),
					bucket_locks_(buckets_.size()), table_nodes_(element_count),
					heap_(element_count), heap_locks_(element_count) {
				if (buckets_.size() == 0)
					throw std::invalid_argument("Can't construct fixed hashtable: insufficient memory");

				for (size_t i = 0; i < table_nodes_.size() - 1; i++)
					table_nodes_[i].next_ = &table_nodes_[i + 1];
				disposed_nodes_head_ = &table_nodes_[0];
			}

		  public:
			CNDC(const CNDC&) = delete;
			CNDC& operator =(const CNDC&) = delete;

			~CNDC() = default;

			size_t capacity() const {
				return max_count_;
			}

			size_t size() const {
				return count_;
			}

			size_t elementSize() const {
				return elementSize(load_factor_);
			}

			void dump(std::ostream& out) const {
				for (size_t i = 0; i < buckets_.size(); i++) {
					out << '(' << i << ')';
					lock_guard_t lg((buckets_[i].lock_));
					HashTableNode* node = buckets_[i].ptr_;

					while (node) {
						out << "->" << node->key_ << '[' << node->value_ << ']';
						node = node->next_;
					}
					out << std::endl;
				}
			}

			optional_value_t find(const key_t& key) {
				size_t bucket_nr = keyToBucketNr(key);
				HashTableNode** node_ref = &buckets_[bucket_nr];
				lock_guard_t lg((bucket_locks_[bucket_nr]));

				while (*node_ref) {
					if (key < (*node_ref)->key_)
						break;
					if (key == (*node_ref)->key_) {
						HashTableNode* found_node = *node_ref;
						value_t value = found_node->value_;
						uint32_t version = found_node->version_;
						lg.unlock();

						heapIncreasePriority(found_node, version);
						return {value};
					}
					node_ref = &((*node_ref)->next_);
				}

				return {};
			}

			bool insert(key_t key, value_t value, size_t priority) {
				auto deleter = [&](HashTableNode* node) {
					disposeNode(node);
				};
				std::unique_ptr<HashTableNode, decltype(deleter)>
						empty_node(acquireFreeHtNode(),
								   deleter); //Returns the node back into the pool in case of failure/exception

				size_t bucket_nr = keyToBucketNr(key);
				HashTableNode** node_ref = &buckets_[bucket_nr];
				lock_guard_t lg((bucket_locks_[bucket_nr]));
				while (*node_ref) {
					if (key < (*node_ref)->key_)
						break;
					if (key == (*node_ref)->key_) {
						HashTableNode* found_node = *node_ref;
						uint32_t version = found_node->version_;
						lg.unlock();

						heapIncreasePriority(found_node, version);
						return false;
					}
					node_ref = &((*node_ref)->next_);
				}

				empty_node->key_ = std::move(key);
				empty_node->value_ = std::move(value);
				empty_node->insertAfter(node_ref);
				heapInsert(empty_node.get(), priority, std::move(lg));
				empty_node.release();
				return true;
			}

		  private:
			static constexpr idx_t null_idx = static_cast<idx_t>(-1);

			size_t keyToBucketNr(const key_t& key) const {
				static hasher_t hasher;
				return hasher(key) % buckets_.size();
			}

			HashTableNode* evictItemWithLowestPriority() {
				HashTableNode* candidate = heapExtractMin();
				if (!candidate)
					return nullptr;

				size_t bucket_nr = keyToBucketNr(candidate->key_);
				HashTableNode** node_ref = &buckets_[bucket_nr];
				lock_guard_t lg((bucket_locks_[bucket_nr]));

				while (*node_ref) {
					if (candidate->key_ < (*node_ref)->key_)
						break;
					if (candidate->key_ == (*node_ref)->key_) {
						HashTableNode* found_node = *node_ref;
						assert(found_node == candidate);
						found_node->extract(node_ref);
						found_node->version_++;
						return found_node;
					}
					node_ref = &(*node_ref)->next_;
				}
				assert(false);
				return nullptr;
			}

			HashTableNode* acquireFreeHtNode() {
				HashTableNode* node = nullptr;
				while (true) {
					if (node = allocHtNode())
						break;
					if (node = evictItemWithLowestPriority())
						break;
				}
				return node;
			}

			HashTableNode* allocHtNode() {
				if (!disposed_nodes_head_)
					return nullptr;

				lock_guard_t lg(disposed_nodes_lock_);
				HashTableNode* tnode = disposed_nodes_head_;
				if (tnode)
					disposed_nodes_head_ = tnode->next_;
				return tnode;
			}

			void disposeNode(HashTableNode* tnode) {
				assert(tnode);
				lock_guard_t lg(disposed_nodes_lock_);
				tnode->next_ = disposed_nodes_head_;
				disposed_nodes_head_ = tnode;
			}

			/**
			 * @attention Assumes a and b nodes are locked
			 */
			void swapHeapNodes(idx_t a, idx_t b) {
				std::swap(heap_[a], heap_[b]);
				heap_[a].ht_node_->heap_node_ = a;
				heap_[b].ht_node_->heap_node_ = b;
			}

			HashTableNode* heapExtractMin() {
				lock_guard_t lg1((heap_locks_[0]));
				if (count_ == 0)
					return nullptr;

				HashTableNode* evicted_node = heap_[0].ht_node_;
				heap_[0].ht_node_ = nullptr;

				if (count_ == 1) {
					count_ = 0;
					return evicted_node;
				}

				{
					lock_guard_t lg2((heap_locks_[count_ - 1]));
					std::swap(heap_[0], heap_[(count_ - 1)]);
					heap_[0].ht_node_->heap_node_ = 0;
					count_--;
				}
				if (count_ > 1) {
					topDownHeapify(0, std::move(lg1));
				}
				return evicted_node;
			}

			void heapInsert(HashTableNode* ht_node, size_t priority, lock_guard_t bucket_lock) {
				assert(ht_node);
				lock_guard_t lg1((heap_locks_[0]));
				assert(count_ < max_count_);
				ht_node->version_++;
				ht_node->heap_node_ = count_.load();

				if (count_ == 0) {
					heap_[count_].ht_node_ = ht_node;
					heap_[count_].priority_ = priority;
					count_ = 1;
				} else {
					lock_guard_t lg2((heap_locks_[count_]));
					heap_[count_].ht_node_ = ht_node;
					heap_[count_].priority_ = priority;

					count_++;
					uint32_t version = ht_node->version_;
					lg1.unlock();
					bucket_lock.unlock();
					bottomUpHeapify(ht_node, std::move(lg2), version);
				}
			}

			void heapIncreasePriority(HashTableNode* ht_node, uint32_t version) {
				auto lg = heapLockNode(ht_node, version);
				if (lg.owns_lock()) {
					idx_t heap_node_idx = ht_node->heap_node_;
					heap_[heap_node_idx].priority_.access();
					topDownHeapify(heap_node_idx, std::move(lg));
				}
			}

			lock_guard_t heapLockNode(HashTableNode* ht_node, uint32_t version) {
				backoff_t backoff;
				while (true) {
					if (ht_node->version_ != version)
						return {};
					idx_t heap_node = ht_node->heap_node_;
					lock_guard_t lg(heap_locks_[heap_node], std::try_to_lock);
					if (lg.owns_lock()) {
						//if (ht_node->version_ == version && heap_[heap_node].ht_node_ == ht_node)
						if (ht_node->version_ != version || heap_[heap_node].ht_node_ == nullptr) //TODO Ensure there's no data races
							return {};
						if (heap_[heap_node].ht_node_ == ht_node) {
							assert(heap_node == ht_node->heap_node_);
							return std::move(lg);
						}
					}
					if (UseBackoff)
						backoff();
				}
			}

			idx_t topDownHeapify(idx_t parent, lock_guard_t parent_lg) {
				assert(parent_lg.owns_lock());
				while (true) {

					auto left = heapAcquireLeftLock(parent);
					if (left.idx == null_idx)
						return parent;

					auto right = heapAcquireRightLock(parent);

					idx_t min = 0;

					if (right.idx != null_idx && heap_[right.idx] < heap_[left.idx]) {
						left.lg.unlock();
						min = right.idx;
					} else {
						right.lg = lock_guard_t(); //Optionally release right lock
						min = left.idx;
					}

					if (heap_[min] < heap_[parent]) {
						swapHeapNodes(parent, min);
						parent_lg = (min == left.idx) ?
									std::move(left.lg) :
									std::move(right.lg);
						parent = min;
					} else
						return parent;
				}
			}

			void bottomUpHeapify(HashTableNode* ht_node, lock_guard_t heap_node_lg, uint32_t version) {
				assert(heap_node_lg.owns_lock());
				idx_t heap_node_idx = ht_node->heap_node_;
				backoff_t backoff;
				while (true) {
					auto parent_idx = heapParent(heap_node_idx);
					if (parent_idx == null_idx)
						return;
					lock_guard_t parent_lg(heap_locks_[parent_idx], std::try_to_lock);
					if (parent_lg.owns_lock()) {
						if (heap_[heap_node_idx].priority_ < heap_[parent_idx].priority_)
							swapHeapNodes(heap_node_idx, parent_idx);
						else
							return;
					}
					heap_node_lg.unlock();
					heap_node_idx = ht_node->heap_node_;

					if (parent_lg.owns_lock())
						heap_node_lg = std::move(parent_lg);
					else {
						backoff();
						heap_node_lg = heapLockNode(ht_node, version);
						if (!heap_node_lg.owns_lock())
							return;
					}
				}
			}

			idx_t heapParent(idx_t i) {
				return i == 0 ? null_idx : (i - 1) / 2;
			}

			idx_t heapLeft(idx_t i) {
				return i * 2 + 1 < max_count_ ? i * 2 + 1 : null_idx;
			}

			idx_t heapRight(idx_t i) {
				return i * 2 + 2 < max_count_ ? i * 2 + 2 : null_idx;
			}

			LockedHeapNode heapAcquireLeftLock(idx_t i) {
				idx_t child = heapLeft(i);
				if (child == null_idx)
					return {null_idx, {}};
				lock_guard_t lg((heap_locks_[child]));
				//Although count_ can be changed concurrently,
				// its value can't pass 'child' idx without locking 'child' mutex
				if (child < count_)
					return {child, std::move(lg)};
				else
					return {null_idx, {}};
			}

			LockedHeapNode heapAcquireRightLock(idx_t i) {
				idx_t child = heapRight(i);
				if (child == null_idx)
					return {null_idx, {}};
				lock_guard_t lg((heap_locks_[child]));
				//Although count_ can be changed concurrently,
				// its value can't pass 'child' idx without locking 'child' mutex
				if (child < count_)
					return {child, std::move(lg)};
				else
					return {null_idx, {}};
			}


			size_t max_count_;
			double load_factor_;
			std::atomic<idx_t> count_;
			HashTableNode* disposed_nodes_head_;
			std::mutex disposed_nodes_lock_;

			std::vector<HashTableNode*> buckets_;
			std::vector<std::mutex> bucket_locks_;
			std::vector<HashTableNode> table_nodes_;
			std::vector<HeapNode> heap_;
			std::vector<std::mutex> heap_locks_;
		};

		template <bool UseBackoff, bool UseShortIndex = true,
				typename PriorityT = utility::WstPriority,
				typename HasherT = mmh2::MurmurHash2<void>>
		struct CNDCTypeHolder {
			template <typename KeyT, typename ValueT>
			using container_t = CNDC<KeyT, ValueT, PriorityT, HasherT, UseBackoff, UseShortIndex>;
		};
	}
}
#endif //NUMDB_CNDC_H
