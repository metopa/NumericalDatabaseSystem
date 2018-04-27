/** @file weighted_search_tree.h
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_WEIGHTED_SEARCH_TREE_H
#define NUMDB_WEIGHTED_SEARCH_TREE_H

#include <vector>
#include <iomanip>
#include <experimental/optional>
#include <limits>
#include <cassert>
#include <ostream>

#include "numdb/priority.h"


namespace numdb {
	namespace containers {
		template <typename KeyT, typename ValueT, typename ComparatorT, int DegradationRate, bool UseShortIndex>
		class WeightedSearchTree {
			using idx_t = std::conditional_t<UseShortIndex, uint32_t, size_t>;
			using key_t = KeyT;
			using value_t = ValueT;
			using priority_t = utility::WstAvlPriority;
			using comparator_t = ComparatorT;
			using optional_value_t = std::experimental::optional<ValueT>;
			static constexpr idx_t null = static_cast<idx_t>(-1);
			static constexpr bool DEGRADE_NODE_ON_SEARCH = DegradationRate > 0;

			struct Node {
				key_t key;
				value_t value;
				priority_t priority;
				idx_t left;
				idx_t right;
				idx_t parent;
				Node(key_t key, value_t value, priority_t priority) :
						key(key), value(value), priority(priority),
						left(null), right(null), parent(null) {}

				int avlBalance() {
					return priority.avlBalance();
				}

				void setAvlBalance(int b) {
					priority.setAvlBalance(b);
				}
			};

		  public:
			static constexpr idx_t maxElemCountForCapacity(idx_t capacity) {
				return capacity / sizeof(Node);
			}

			static constexpr idx_t elementSize() {
				return sizeof(Node);
			}

			static constexpr bool isThreadsafe() {
				return false;
			}

			WeightedSearchTree(size_t available_memory, comparator_t comparator = {}) :
					max_node_count_(maxElemCountForCapacity(available_memory)),
					node_count_(0), root_idx_(null), comparator_(std::move(comparator)) {
				if (UseShortIndex && max_node_count_ > std::numeric_limits<idx_t>::max() - 1)
					throw std::length_error(
							"Capacity (64-bit) exceeded max index value (32-bit). Disable short index optimization (UseShortIndex template param)");

				data_.reserve(max_node_count_);
			}

			WeightedSearchTree(const WeightedSearchTree&) = delete;
			WeightedSearchTree& operator =(const WeightedSearchTree&) = delete;

			idx_t capacity() const {
				return max_node_count_;
			}

			idx_t size() const {
				return node_count_;
			}

			optional_value_t find(const key_t& key) {
				idx_t idx = treeSearch(key, true);
				if (idx != null)
					return {_(idx).value};
				else
					return {};
			}

			bool remove(const key_t& key) {
				idx_t node = treeSearch(key, false);
				if (node == null)
					return false;
				assert(node_count_ > 0);
				node = heapRemove(node);
				treeRemove(node);
				assert(checkInvariants());
				return true;
			}

			bool insert(key_t key, value_t value, size_t priority) {
				idx_t node;
				if (node_count_ == max_node_count_) {
					node = heapRemove(0);
					treeRemove(node);
					assert(checkInvariants());
					_(node) = Node(std::move(key), std::move(value), priority);
				} else {
					node = (idx_t) data_.size();
					data_.push_back(Node(std::move(key), std::move(value), priority));
				}
				_(node).priority.access();
				node_count_++;
				treeInsert(node);
				bottomUpHeapify(node);
				assert(checkInvariants());
				return true;
			}

			void dump(std::ostream& out) {
				dumpTree(root_idx_, out, 0);
				dumpHeap(0, out, 0);
			}

			bool checkInvariants() {
				assert((node_count_ == 0) == (root_idx_ == null));
				checkAvlInvariant(root_idx_);
				if (node_count_ > 0)
					checkHeapInvariant(0);
				return true;
			}

		  private:
			void dumpTree(idx_t node, std::ostream& out, int level) {
				for (int i = 0; i < level; i++)
					out << "- ";
				if (node == null)
					out << "#" << std::endl;
				else {
					out << _(node).key << "->" << _(node).avlBalance() << std::endl;
					if (_(node).left != null || _(node).right != null) {
						dumpTree(_(node).left, out, level + 1);
						dumpTree(_(node).right, out, level + 1);
					}
				}
			}

			void dumpHeap(idx_t node, std::ostream& out, int level) {
				for (int i = 0; i < level; i++)
					out << ". ";
				if (node == null)
					out << "#" << std::endl;
				else {
					out << _(node).key << "->" << _(node).priority.value() / 256
						<< ',' << _(node).priority.value() % 256 << std::endl;
					if (heapLeft(node) != null || heapRight(node) != null) {
						dumpHeap(heapLeft(node), out, level + 1);
						dumpHeap(heapRight(node), out, level + 1);
					}
				}
			}

			int checkAvlInvariant(idx_t node) {
				if (node == null)
					return 0;
				int left = checkAvlInvariant(_(node).left);
				int right = checkAvlInvariant(_(node).right);
				assert(right - left == _(node).avlBalance());
				if (_(node).left != null) {
					assert(comparator_(_(_(node).left).key, _(node).key));
					assert(_(_(node).left).parent == node);
				}
				if (_(node).right != null) {
					assert(comparator_(_(node).key, _(_(node).right).key));
					assert(_(_(node).right).parent == node);
				}

				return std::max(left, right) + 1;
			}

			void checkHeapInvariant(idx_t node) {
				idx_t child = heapLeft(node);
				if (child != null) {
					assert(!(_(child).priority < _(node).priority));
					checkHeapInvariant(child);
				}

				child = heapRight(node);
				if (child != null) {
					assert(!(_(child).priority < _(node).priority));
					checkHeapInvariant(child);
				}
			}

			idx_t treeSearch(const key_t& key, bool degrade_priority) {
				idx_t n = root_idx_;
				while (n != null) {
					if (comparator_(key, _(n).key)) {
						if (DEGRADE_NODE_ON_SEARCH && degrade_priority) {
							_(n).priority.visit(DegradationRate);
							n = bottomUpHeapify(n);
						}
						n = _(n).left;
					} else if (comparator_(_(n).key, key)) {
						if (DEGRADE_NODE_ON_SEARCH && degrade_priority) {
							_(n).priority.visit(DegradationRate);
							n = bottomUpHeapify(n);
						}
						n = _(n).right;
					} else {
						_(n).priority.access();
						n = topDownHeapify(n);

						return n;
					}
				}
				return null;
			}

			void treeInsert(idx_t node) {
				idx_t parent = root_idx_;
				if (parent == null) {
					root_idx_ = node;
					return;
				}
				while (1) {
					if (comparator_(_(node).key, _(parent).key)) {
						if (_(parent).left == null) {
							_(parent).left = node;
							_(node).parent = parent;
							treeBalanceAfterInsert(parent, -1);
							return;
						} else
							parent = _(parent).left;
					} else if (comparator_(_(parent).key, _(node).key)) {
						if (_(parent).right == null) {
							_(parent).right = node;
							_(node).parent = parent;
							treeBalanceAfterInsert(parent, 1);
							return;
						} else
							parent = _(parent).right;
					} else {
						//node.key == parent.key
						assert(!"Unreachable code");
						return;
					}
				}
			}

			idx_t treeRemove(idx_t node) {
				assert(node != null);
				if (_(node).left == null) {
					int delta = isLeftSon(node) ? 1 : -1;
					treeUpdateParent(node, _(node).right);
					treeBalanceAfterRemove(_(node).parent, delta);
					_(node).right = null;
					_(node).parent = null;
					return node;
				}
				if (_(node).right == null) {
					int delta = isLeftSon(node) ? 1 : -1;
					treeUpdateParent(node, _(node).left);
					treeBalanceAfterRemove(_(node).parent, delta);
					_(node).left = null;
					_(node).parent = null;
					return node;
				}

				idx_t predecessor = treeRemove(getPredecessor(node));
				assert(predecessor != null);

				_(predecessor).left = _(node).left;
				if (_(predecessor).left != null)
					_(_(predecessor).left).parent = predecessor;

				_(predecessor).right = _(node).right;
				if (_(predecessor).right != null)
					_(_(predecessor).right).parent = predecessor;

				_(predecessor).setAvlBalance(_(node).avlBalance());

				treeUpdateParent(node, predecessor);

				_(node).right = _(node).left = _(node).parent = null;
				return node;
			}

			idx_t getPredecessor(idx_t node) {
				assert(node != null);
				assert(_(node).left != null);
				node = _(node).left;
				while (_(node).right != null)
					node = _(node).right;
				return node;
			}

			void treeBalanceAfterInsert(idx_t node, int delta) {
				while (node != null) {
					int temp_balance = _(node).avlBalance() + delta;

					switch (temp_balance) {
						case 0:
							_(node).setAvlBalance(0);
							return;
						case -2:
							if (_(_(node).left).avlBalance() == -1)
								rotateRight(node);
							else
								rotateLeftRight(node);
							return;
						case 2:
							if (_(_(node).right).avlBalance() == 1)
								rotateLeft(node);
							else
								rotateRightLeft(node);
							return;
						case 1:
						case -1:
							_(node).setAvlBalance(temp_balance);
							if (_(node).parent != null)
								delta = _(_(node).parent).left == node ? -1 : 1;
							node = _(node).parent;
					}
				}
			}

			void treeBalanceAfterRemove(idx_t node, int delta) {
				while (node != null) {
					int temp_balance = _(node).avlBalance() + delta;

					switch (temp_balance) {
						case -2:
							if (_(_(node).left).avlBalance() <= 0) {
								node = rotateRight(node); //adjust balance!
								if (_(node).avlBalance() == 1)
									return;
							} else
								node = rotateLeftRight(node);
							break;
						case 2:
							if (_(_(node).right).avlBalance() >= 0) {
								node = rotateLeft(node); //adjust balance!
								if (_(node).avlBalance() == -1)
									return;
							} else
								node = rotateRightLeft(node);
							break;
						case -1:
						case 1:
							_(node).setAvlBalance(temp_balance);
							return;
						case 0:
							_(node).setAvlBalance(temp_balance);
							break;
					}
					delta = isLeftSon(node) ? 1 : -1;
					node = _(node).parent;
				}
			}

			bool isLeftSon(idx_t node) {
				return _(node).parent != null &&
					   _(_(node).parent).left == node;
			}

			void treeUpdateParent(idx_t current, idx_t next) {
				if (next != null) {
					_(next).parent = _(current).parent;
				}

				if (_(current).parent == null) {
					assert(root_idx_ == current);
					root_idx_ = next;
				} else {
					if (_(_(current).parent).left == current)
						_(_(current).parent).left = next;
					else {
						assert(_(_(current).parent).right == current);
						_(_(current).parent).right = next;
					}
				}
			}

			idx_t rotateLeftImpl(idx_t parent) {
				idx_t right = _(parent).right;
				assert(right != null);
				_(right).parent = _(parent).parent;
				_(parent).parent = right;
				_(parent).right = _(right).left;
				_(right).left = parent;
				if (_(right).parent == null) {
					assert(root_idx_ == parent);
					root_idx_ = right;
				} else {
					if (_(_(right).parent).left == parent)
						_(_(right).parent).left = right;
					else {
						assert(_(_(right).parent).right == parent);
						_(_(right).parent).right = right;
					}
				}

				if (_(parent).right != null) {
					assert(_(_(parent).right).parent == right);
					_(_(parent).right).parent = parent;
				}
				return right;
			}
			idx_t rotateRightImpl(idx_t parent) {
				idx_t left = _(parent).left;
				assert(left != null);
				_(left).parent = _(parent).parent;
				_(parent).parent = left;
				_(parent).left = _(left).right;
				_(left).right = parent;
				if (_(left).parent == null) {
					assert(root_idx_ == parent);
					root_idx_ = left;
				} else {
					if (_(_(left).parent).left == parent)
						_(_(left).parent).left = left;
					else {
						assert(_(_(left).parent).right == parent);
						_(_(left).parent).right = left;
					}
				}

				if (_(parent).left != null) {
					assert(_(_(parent).left).parent == left);
					_(_(parent).left).parent = parent;
				}
				return left;
			}
			idx_t rotateLeft(idx_t parent) {
				parent = rotateLeftImpl(parent);
				_(parent).setAvlBalance(_(parent).avlBalance() - 1);
				_(_(parent).left).setAvlBalance(-_(parent).avlBalance());
				return parent;
			}
			idx_t rotateRight(idx_t parent) {
				parent = rotateRightImpl(parent);
				_(parent).setAvlBalance(_(parent).avlBalance() + 1);
				_(_(parent).right).setAvlBalance(-_(parent).avlBalance());
				return parent;
			}
			idx_t rotateLeftRight(idx_t parent) {
				rotateLeftImpl(_(parent).left);
				parent = rotateRightImpl(parent);
				switch (_(parent).avlBalance()) {
					case -1:
						_(_(parent).left).setAvlBalance(0);
						_(_(parent).right).setAvlBalance(1);
						break;
					case 0:
						_(_(parent).left).setAvlBalance(0);
						_(_(parent).right).setAvlBalance(0);
						break;
					case 1:
						_(_(parent).left).setAvlBalance(-1);
						_(_(parent).right).setAvlBalance(0);
						break;
				}
				_(parent).setAvlBalance(0);
				return parent;
			}
			idx_t rotateRightLeft(idx_t parent) {
				rotateRightImpl(_(parent).right);
				parent = rotateLeftImpl(parent);
				switch (_(parent).avlBalance()) {
					case -1:
						_(_(parent).left).setAvlBalance(0);
						_(_(parent).right).setAvlBalance(1);
						break;
					case 0:
						_(_(parent).left).setAvlBalance(0);
						_(_(parent).right).setAvlBalance(0);
						break;
					case 1:
						_(_(parent).left).setAvlBalance(-1);
						_(_(parent).right).setAvlBalance(0);
						break;
				}
				_(parent).setAvlBalance(0);
				return parent;
			}

			void replaceTreeReferences(idx_t old_node, idx_t new_node,
									   idx_t parent, idx_t left, idx_t right,
									   bool is_left_son) {
				if (parent != null) {
					if (is_left_son)
						_(parent).left = new_node;
					else {
						assert(_(parent).right == old_node);
						_(parent).right = new_node;
					}
				}
				if (left != null) {
					assert(_(left).parent == old_node);
					_(left).parent = new_node;
				}
				if (right != null) {
					assert(_(right).parent == old_node);
					_(right).parent = new_node;
				}
			}

			void swapNodes(idx_t a, idx_t b) {
				idx_t b_parent = _(b).parent;
				idx_t b_left = _(b).left;
				idx_t b_right = _(b).right;
				bool b_is_left_son = isLeftSon(b);
				replaceTreeReferences(a, b, _(a).parent, _(a).left, _(a).right, isLeftSon(a));
				replaceTreeReferences(b, a, b_parent, b_left, b_right, b_is_left_son);
				if (root_idx_ == a)
					root_idx_ = b;
				else if (root_idx_ == b)
					root_idx_ = a;
				std::swap(_(a), _(b));
			}

			idx_t heapRemove(idx_t node) {
				if (node != node_count_ - 1) {
					swapNodes(node, node_count_ - 1);
					node_count_--;
					if (node_count_ > 0) {
						idx_t new_idx = topDownHeapify(node);
						if (new_idx == node)
							bottomUpHeapify(new_idx);
					}
				} else
					node_count_--;
				return node_count_;
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
				return i * 2 + 1 < node_count_ ? i * 2 + 1 : null;
			}
			idx_t heapRight(idx_t i) {
				return i * 2 + 2 < node_count_ ? i * 2 + 2 : null;
			}

			/// \brief Access i-th node in the data_ array
			Node& _(idx_t i) {
				assert(i < data_.size());
				return data_[i];
			}
			const Node& _(idx_t i) const {
				assert(i < data_.size());
				return data_[i];
			}

			const size_t max_node_count_;
			size_t node_count_;
			size_t root_idx_;
			std::vector<Node> data_;
			comparator_t comparator_;
		};

		template <int DegradationRate, bool UseShortIndex = true, typename ComparatorT = std::less<>>
		struct WeightedSearchTreeTypeHolder {
			template <typename KeyT, typename ValueT>
			using container_t = WeightedSearchTree<KeyT, ValueT, ComparatorT, DegradationRate, UseShortIndex>;
		};
	}
}
#endif //NUMDB_WEIGHTED_SEARCH_TREE_H
