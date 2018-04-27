/** @file fixed_hashtable_base.h
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_FIXED_HASHTABLE_BASE_H
#define NUMDB_FIXED_HASHTABLE_BASE_H

#include <functional>
#include <cmath>
#include <vector>
#include <ostream>
#include <cassert>
#include <experimental/optional>

#include "numdb/utils.h"

namespace numdb {
	namespace containers {
//TODO Restrict class to use a power of 2 as a table size
//TODO Use bit masking instead of integer division
//TODO Preallocate all nodes
		template <typename CrtpDerived>
		class FixedHashtableBase {
			friend CrtpDerived;
		  public:
			using traits = CacheContainerTraits<CrtpDerived>;
			using key_t = typename traits::key_t;
			using value_t = typename traits::value_t;
			using hasher_t = typename traits::hasher_t;
			using node_base_t = typename traits::node_base_t;
			using optional_value_t = std::experimental::optional<value_t>;

			class Node : public node_base_t {
				friend class FixedHashtableBase;

			  public:
				Node(key_t key, value_t value, Node* next = nullptr) :
						next_(next), key_(std::move(key)), value_(std::move(value)) {}

				~Node() {
					delete next_;
				}

				key_t& key() { return key_; }
				const key_t& key() const { return key_; }

				value_t& value() { return value_; }
				const value_t& value() const { return value_; }

			  private:
				void extract(Node** this_node_ref) {
					assert(this_node_ref != nullptr && this == *this_node_ref);
					*this_node_ref = next_;
				}

				void insertBefore(Node** next_node_ref) {
					assert(next_node_ref != nullptr);
					next_ = *next_node_ref;
					*next_node_ref = this;
				}

				Node* next_;
				key_t key_;
				value_t value_;
			};

			static constexpr size_t maxElemCountForCapacity(
					size_t capacity, double load_factor) {
				return static_cast<size_t>(
						capacity / (sizeof(Node) + sizeof(Node*) / load_factor));
			}

			static constexpr size_t elementSize(double load_factor) {
				return static_cast<size_t>(
						sizeof(Node) +
						std::ceil(sizeof(Node*) / load_factor) +
						additionalNodeOverhead());
			}

			FixedHashtableBase(size_t available_memory, double load_factor) :
					FixedHashtableBase(
							maxElemCountForCapacity(available_memory, load_factor),
							load_factor, 0) {}

		  private:
			FixedHashtableBase(size_t element_count, double load_factor, char dummy) :
					buckets_(static_cast<size_t>(element_count / load_factor), nullptr),
					max_count_(element_count),
					load_factor_(load_factor),
					count_(0) {
				if (buckets_.size() == 0)
					throw std::invalid_argument("Can't construct fixed hashtable: insufficient memory");
			}

		  public:
			FixedHashtableBase(const FixedHashtableBase&) = delete;
			FixedHashtableBase& operator =(const FixedHashtableBase&) = delete;

			~FixedHashtableBase() {
				for (Node* n : buckets_)
					delete n;
			}

			constexpr static bool isThreadsafe() {
				return false;
			}

			optional_value_t find(const key_t& key) {
				Node** root_node = &buckets_[getBucket(key)];
				Node** node_ref = root_node;

				while (*node_ref) {
					if ((*node_ref)->key_ == key) {
						Node* found_node = *node_ref;

						if (root_node != node_ref) {
							found_node->extract(node_ref);
							found_node->insertBefore(root_node);
						}
						return {found_node->value_};
					}
					node_ref = &((*node_ref)->next_);
				}

				return {};
			}

			size_t getBucket(const key_t& key) const {
				size_t bucket = hasher_t()(key) % buckets_.size();
				return bucket;
			}

			bool insert(key_t key, value_t value, size_t priority) {
				Node** root_node = &buckets_[getBucket(key)];
				Node** node = root_node;
				while (*node) {
					if ((*node)->key_ == key) {
						nodeAccessed(*node);
						return false;
					}
					node = &((*node)->next_);
				}

				Node* new_node = nullptr;
				if (count_ == max_count_) {
					new_node = extractLuNode();
					new_node->key_ = std::move(key);
					new_node->value_ = std::move(value);
				} else
					new_node = new Node(std::move(key), std::move(value));

				new_node->insertBefore(root_node);
				nodeInserted(new_node, priority);

				return true;
			}

			Node* extractNode(const key_t& key) {
				Node** node_ref = &buckets_[getBucket(key)];

				while (*node_ref) {
					if ((*node_ref)->key_ == key) {
						Node* found_node = *node_ref;
						found_node->extract(node_ref);
						nodeExtracted(found_node);
						return found_node;
					}
					node_ref = &((*node_ref)->next_);
				}

				return nullptr;
			}

			bool erase(const key_t& key) {
				Node* n = extractNode(key);
				if (!n)
					return false;
				delete n;
				return true;
			}

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
					out << '[' << i << ']';
					Node* node = buckets_[i];
					while (node) {
						out << "->" << node->key_ << '[' << node->value_ << ']';
						node = node->next_;
					}
					out << std::endl;
				}
			}

		  private:
			void nodeAccessed(Node* node) {
				static_cast<CrtpDerived*>(this)->nodeAccessedImpl(node);
			}

			void nodeInserted(Node* node, size_t priority) {
				count_++;
				assert(count_ <= max_count_);
				static_cast<CrtpDerived*>(this)->nodeInsertedImpl(node, priority);
			}

			void nodeExtracted(Node* node) {
				assert(count_ > 0);
				count_--;
				static_cast<CrtpDerived*>(this)->nodeExtractedImpl(node);
			}

			Node* extractLuNode() {
				assert(count_ == max_count_);
				Node* candidate = static_cast<CrtpDerived*>(this)->getLuNodeImpl();
				assert(candidate != nullptr);
				Node** node_ref = &buckets_[getBucket(candidate->key_)];
				///Node forms a single linked list, so we need to search for the node again
				while (*node_ref) {
					if ((*node_ref)->key_ == candidate->key_) {
						Node* found_node = *node_ref;
						assert(found_node == candidate);
						found_node->extract(node_ref);
						assert(count_ > 0);
						count_--;
						return found_node;
					}
					node_ref = &((*node_ref)->next_);
				}
				assert(false);
				return nullptr;
			}

			static size_t additionalNodeOverhead() {
				return CrtpDerived::additionalNodeOverheadImpl();
			}

		  protected:
			///Following methods would be implemented in derived classes.
			///They are called through Curiously Recurring Template Pattern
			size_t additionalNodeOverheadImpl() = delete;
			void nodeAccessedImpl(Node* node) = delete;
			void nodeInsertedImpl(Node* node, size_t priority) = delete;
			void nodeExtractedImpl(Node* node) = delete;
			Node* getLuNodeImpl() = delete;

			std::vector<Node*> buckets_;
			const size_t max_count_;
			const double load_factor_;
			size_t count_;
		};
	}
}

#endif //NUMDB_FIXED_HASHTABLE_BASE_H
