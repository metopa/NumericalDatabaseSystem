/** @file splay_tree_base.h
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_SPLAY_TREE_BASE_H
#define NUMDB_SPLAY_TREE_BASE_H

#include <functional>
#include <cassert>
#include <ostream>
#include <experimental/optional>

#include "numdb/utils.h"


namespace numdb {
	namespace containers {
		template <typename NodeT, bool IsReal>
		class RefToSelfPolicy;

		template <typename NodeT>
		class RefToSelfPolicy<NodeT, false> {
		  public:
			static constexpr bool hasRefToSelf() { return false; }
			void setRefToSelf(NodeT** ptr) {}
			NodeT** getRefToSelf() { return nullptr; }
		};

		template <typename NodeT>
		class RefToSelfPolicy<NodeT, true> {
			NodeT** ptr_ = nullptr;
		  public:
			static constexpr bool hasRefToSelf() { return true; }
			void setRefToSelf(NodeT** ptr) { ptr_ = ptr; }
			NodeT** getRefToSelf() { return ptr_; }
		};

//TODO Allocate memory in a single block
//TODO Add removal strategy
//TODO Add timestamp
//TODO Add check for splay strategy methods
		template <typename CrtpDerived>
		class SplayTreeBase {
			friend CrtpDerived;
		  public:
			using traits = CacheContainerTraits<CrtpDerived>;
			using key_t = typename traits::key_t;
			using value_t = typename traits::value_t;
			using comparator_t = typename traits::comparator_t;
			using strategy_t = typename traits::strategy_t;
			using node_base_t = typename traits::node_base_t;
			using optional_value_t = std::experimental::optional<value_t>;

			//Empty member optimization http://www.cantrip.org/emptyopt.html
			class Node :
					public strategy_t,
					public node_base_t,
					public RefToSelfPolicy<Node, traits::enableRefToSelf()> {
			  public:
				friend class SplayTreeBase;

				friend CrtpDerived;

				Node(key_t key, value_t value, uint64_t priority) :
						strategy_t(priority),
						key_(std::move(key)),
						value_(std::move(value)),
						left_(nullptr), right_(nullptr) {}

				~Node() {
					delete left_;
					delete right_;
				}

				key_t& key() {
					return key_;
				}

				value_t& value() {
					return value_;
				}

			  private:
				static void dump(const Node* node, std::ostream& out, int level) {
					for (int i = 0; i < level; i++)
						out << "- ";
					if (!node)
						out << "@" << std::endl;
					else {
						out << node->key_ << "->" << node->value_ << " ";
						node->dumpStrategy(out);
						out << std::endl;
						if (node->left_ || node->right_) {
							dump(node->left_, out, level + 1);
							dump(node->right_, out, level + 1);
						}
					}
				}

				// This member ordering is designed to keep
				// the most frequently accessed members
				// in the beginning of the object
				key_t key_;
				Node* left_;
				Node* right_;
				value_t value_;
			};

			static constexpr size_t maxElemCountForCapacity(size_t capacity) {
				return capacity / sizeof(Node);
			}

			static constexpr size_t elementSize() {
				return sizeof(Node);
			}

			static constexpr bool isThreadsafe() {
				return false;
			}

			SplayTreeBase(size_t available_memory, comparator_t&& comparator) :
					root_(nullptr), node_count_(0),
					max_node_count_(maxElemCountForCapacity(available_memory)),
					comparator_(std::move(comparator)) {}

			SplayTreeBase(const SplayTreeBase&) = delete;
			SplayTreeBase& operator =(const SplayTreeBase&) = delete;

			~SplayTreeBase() {
				delete root_;
			}

			size_t capacity() const {
				return max_node_count_;
			}

			size_t size() const {
				return node_count_;
			}

			void verifyRefToSelfIntegrity() {
				if (Node::hasRefToSelf()) {
					verifyRefToSelfIntegrityImpl(root_);
				}
			}

			size_t computeTreeHeight() {
				return computeTreeHeightImpl(root_);
			}

			optional_value_t find(const key_t& key) {
				EChildType tmp;
				Node* node = findImplSplay(key, root_, tmp, true, true);
				if (!node)
					return {};
				else
					return {node->value_};
			}

			Node* extractNode(const key_t& key) {
				Node* extracted = extractNodeImpl(findRefImpl(key, root_));
				if (extracted)
					nodeExtracted(extracted);
				return extracted;
			}

			bool remove(const key_t& key) {
				Node* n = extractNode(key);
				if (!n)
					return false;
				delete n;
				return true;
			}

			/**
			 * @brief
			 * @param key
			 * @param value
			 * @warning insert assumes that there's no such key exists and does no additional checks
			 * @return
			 */
			bool insert(key_t key, value_t value, uint64_t priority) {
				Node* node;
				if (node_count_ < max_node_count_)
					node = new Node(std::move(key), std::move(value), priority);
				else {
					node = extractLuNode(key);
					assert(node != nullptr);
					node->key_ = std::move(key);
					node->value_ = std::move(value);
				}

				Node*& place_to_insert = findRefImpl(key, root_);
				assert(place_to_insert == nullptr);
				place_to_insert = node;
				node->setRefToSelf(&place_to_insert);
				nodeInserted(node);
				return true;
			}

			void dump(std::ostream& out) {
				Node::dump(root_, out, 0);
			}

		  private:
			enum class EChildType {
				LEFT, RIGHT, UNDEFINED, DONT_SPLAY
			};

			Node* extractNodeImpl(Node*& node_ref) {
				Node* node = node_ref;
				if (!node)
					return nullptr;

				if (!node->left_) {
					node_ref = node->right_; //Can be nullptr
					if (node_ref)
						node_ref->setRefToSelf(&node_ref);
					node->right_ = nullptr;
					return node;
				}
				if (!node->right_) {
					node_ref = node->left_;
					node_ref->setRefToSelf(&node_ref);
					node->left_ = nullptr;
					return node;
				}

				Node* predecessor = extractNodeImpl(getPredecessor(node));
				assert(predecessor != nullptr);

				predecessor->left_ = node->left_;
				if (predecessor->left_)
					predecessor->left_->setRefToSelf(&(predecessor->left_));

				predecessor->right_ = node->right_;
				if (predecessor->right_)
					predecessor->right_->setRefToSelf(&(predecessor->right_));

				node_ref = predecessor;
				node_ref->setRefToSelf(&node_ref);

				node->right_ = node->left_ = nullptr;
				return node;
			}

			Node*& findRefImpl(const key_t& key, Node*& node) {
				if (!node)
					return node;
				if (comparator_(key, node->key_))
					return findRefImpl(key, node->left_);
				else if (comparator_(node->key_, key))
					return findRefImpl(key, node->right_);

				else /*key == node->key_*/
					return node;
			}

			Node* findImplSplay(const key_t& key, Node*& node,
								EChildType& child_type, bool is_root, bool mark_visited) {
				if (!node)
					return nullptr;

				Node* result;
				EChildType grandchild = EChildType::UNDEFINED;

				assert(!node->hasRefToSelf() || node->getRefToSelf() == &node);

				if (comparator_(key, node->key_)) {
					if (mark_visited)
						nodeVisited(node);
					child_type = EChildType::LEFT;
					result = findImplSplay(key, node->left_, grandchild, false, mark_visited);
				} else if (comparator_(node->key_, key)) {
					if (mark_visited)
						nodeVisited(node);
					child_type = EChildType::RIGHT;
					result = findImplSplay(key, node->right_, grandchild, false, mark_visited);
				} else /*key == node->key_*/ {
					if (mark_visited)
						nodeAccessed(node);
					return node;
				}

				if (result)
					child_type = splay(node, is_root, child_type, grandchild);

				return result;
			}

			void rotateLeft(Node*& parent_ref) {
				Node* parent = parent_ref;
				assert(parent != nullptr);
				Node* child = parent->right_;
				assert(child != nullptr);
				parent_ref = child;
				parent->right_ = child->left_;
				if (parent->right_)
					parent->right_->setRefToSelf(&(parent->right_));
				child->left_ = parent;
				child->setRefToSelf(parent->getRefToSelf());
				parent->setRefToSelf(&(child->left_));
			}

			void rotateRight(Node*& parent_ref) {
				Node* parent = parent_ref;
				assert(parent != nullptr);
				Node* child = parent->left_;
				assert(child != nullptr);
				parent_ref = child;
				parent->left_ = child->right_;
				if (parent->left_)
					parent->left_->setRefToSelf(&(parent->left_));
				child->right_ = parent;
				child->setRefToSelf(parent->getRefToSelf());
				parent->setRefToSelf(&(child->right_));
			}

			EChildType splay(Node*& node, bool is_root,
							 EChildType parent_grandparent,
							 EChildType child_parent) {
				if (child_parent == EChildType::DONT_SPLAY ||
					parent_grandparent == EChildType::DONT_SPLAY)
					return EChildType::DONT_SPLAY;

				switch (parent_grandparent) {
					case EChildType::LEFT:
						switch (child_parent) {
							case EChildType::LEFT:
								if (!node->shouldSplay(node->left_->left_)) {
									splay(node->left_, true, child_parent,
										  EChildType::UNDEFINED);
									return EChildType::DONT_SPLAY;
								}
								rotateRight(node);
								rotateRight(node);
								return EChildType::UNDEFINED;
							case EChildType::RIGHT:
								if (!node->shouldSplay(node->left_->right_)) {
									splay(node->left_, true, child_parent,
										  EChildType::UNDEFINED);
									return EChildType::DONT_SPLAY;
								}
								rotateLeft(node->left_);
								rotateRight(node);
								return EChildType::UNDEFINED;

							case EChildType::UNDEFINED:
								if (!node->shouldSplay(node->left_))
									return EChildType::DONT_SPLAY;

								if (is_root) {
									rotateRight(node);
									return EChildType::UNDEFINED;
								} else
									return parent_grandparent;
						}

					case EChildType::RIGHT:
						switch (child_parent) {
							case EChildType::LEFT:
								if (!node->shouldSplay(node->right_->left_)) {
									splay(node->right_, true, child_parent,
										  EChildType::UNDEFINED);
									return EChildType::DONT_SPLAY;
								}
								rotateRight(node->right_);
								rotateLeft(node);
								return EChildType::UNDEFINED;
							case EChildType::RIGHT:
								if (!node->shouldSplay(node->right_->right_)) {
									splay(node->right_, true, child_parent,
										  EChildType::UNDEFINED);
									return EChildType::DONT_SPLAY;
								}
								rotateLeft(node);
								rotateLeft(node);
								return EChildType::UNDEFINED;
							case EChildType::UNDEFINED:
								if (!node->shouldSplay(node->right_))
									return EChildType::DONT_SPLAY;

								if (is_root) {
									rotateLeft(node);
									return EChildType::UNDEFINED;
								} else
									return parent_grandparent;
						}
					case EChildType::UNDEFINED:
						return EChildType::UNDEFINED;
				}
				assert(0 && "Should never be reached");
				return EChildType::DONT_SPLAY;
			}

			Node*& getPredecessor(Node*& node) {
				Node** pred = &(node->left_);
				while ((*pred)->right_)
					pred = &((*pred)->right_);
				return *pred;
			}

			void verifyRefToSelfIntegrityImpl(Node*& node) {
				if (node) {
					assert(node->getRefToSelf() == &node);
					verifyRefToSelfIntegrityImpl(node->left_);
					verifyRefToSelfIntegrityImpl(node->right_);
				}
			}

			size_t computeTreeHeightImpl(Node* node) {
				return node ? std::max(
						computeTreeHeightImpl(node->left_),
						computeTreeHeightImpl(node->right_)) + 1 : 0;
			}

		  private:
			void nodeAccessed(Node* node) {
				node->accessed();
				static_cast<CrtpDerived*>(this)->nodeAccessedImpl(node);
			}

			void nodeVisited(Node* node) {
				node->visited();
				static_cast<CrtpDerived*>(this)->nodeVisitedImpl(node);
			}

			void nodeInserted(Node* node) {
				node_count_++;
				assert(node_count_ <= max_node_count_);
				static_cast<CrtpDerived*>(this)->nodeInsertedImpl(node);
			}

			void nodeExtracted(Node* node) {
				assert(node_count_ > 0);
				node_count_--;
				static_cast<CrtpDerived*>(this)->nodeExtractedImpl(node);
			}

			Node* extractLuNode(const key_t& key) {
				assert(node_count_ == max_node_count_);
				assert(node_count_ > 0);
				Node** candidate = static_cast<CrtpDerived*>(this)->getLuNodeRefImpl(key);
				assert(candidate != nullptr);
				assert(*candidate != nullptr);
				node_count_--;
				return extractNodeImpl(*candidate);
			}

		  protected:
			///Following methods would be implemented in derived classes.
			///They are called through Curiously Recurring Template Pattern
			void nodeAccessedImpl(Node* node) = delete;
			void nodeVisitedImpl(Node* node) = delete;
			void nodeInsertedImpl(Node* node) = delete;
			void nodeExtractedImpl(Node* node) = delete;
			Node** getLuNodeRefImpl(const key_t& key) = delete;

			Node* root_;
			size_t node_count_;
			const size_t max_node_count_;
			comparator_t comparator_;
		};
	}
}
#endif //NUMDB_SPLAY_TREE_BASE_H
