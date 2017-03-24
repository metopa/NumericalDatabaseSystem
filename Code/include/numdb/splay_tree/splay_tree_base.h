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

template <typename NodeT>
class mockParentPointer {
  public:
	static constexpr bool hasParentPtr() {
		return false;
	}
	void setParent(NodeT* ptr) {}
	NodeT* getParent() {
		return nullptr;
	}
};

template <typename NodeT>
class parentPointer {
	NodeT* ptr_ = nullptr;
  public:
	static constexpr bool hasParentPtr() {
		return true;
	}
	void setParent(NodeT* ptr) {
		ptr_ = ptr;
	}
	NodeT* getParent() {
		return ptr_;
	}
};

//TODO Allocate memory in a single block
//TODO Add removal strategy
//TODO Add timestamp
//TODO Add check for splay strategy methods
template <typename CrtpDerived>
class SplayTreeBase {
  public:
	using traits = CacheContainerTraits<CrtpDerived>;
	using key_t = typename traits::key_t;
	using value_t = typename traits::value_t;
	using comparator_t = typename traits::comparator_t;
	using strategy_t = typename traits::strategy_t;
	using node_base_t = typename traits::node_base_t;
	using optional_value_t = std::experimental::optional<value_t>;

	//Empty member optimization http://www.cantrip.org/emptyopt.html
	class Node : public strategy_t, public node_base_t {
		friend class SplayTreeBase;

		Node(key_t key, value_t value,
			 Node* left = nullptr, Node* right = nullptr,
			 strategy_t strategy = strategy_t()) :
				strategy_t(strategy),
				key_(std::move(key)),
				value_(std::move(value)),
				left_(left), right_(right) {}

		~Node() {
			delete left_;
			delete right_;
		}

		const key_t& key() const {
			return key_;
		}

		key_t& key() {
			return key_;
		}

		const value_t& value() const {
			return value_;
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

	  private:
		// This ordering is intended to keep
		// the most frequently accessed members
		// at the beginning of the object
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

	optional_value_t find(const key_t& key) {
		EChildType tmp;
		Node* node = findImplSplay(key, root_, tmp, true);
		if (!node)
			return {};
		else
			return {node->value_};
	}

	Node* extractNode(const key_t& key) {
		return extractNodeImpl(findImpl(key, root_));
	}

	bool remove(const key_t& key) {
		Node* n = extractNode(key);
		if (!n)
			return false;
		delete n;
		return true;
	}

	bool insert(value_t key, value_t value) {
		return insertNode(new Node(std::move(key), std::move(value)));
	}

	bool insertNode(Node* node) {
		Node*& place_to_insert = findImpl(node->key_, root_);
		if (place_to_insert)
			return false;
		node_count_++;
		place_to_insert = node;
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

		node_count_--;

		if (!node->left_) {
			node_ref = node->right_; //Can be nullptr
			node->right_ = nullptr;
			return node;
		}
		if (!node->right_) {
			node_ref = node->left_;
			node->left_ = nullptr;
			return node;
		}

		Node* predecessor = extractNode(getPredecessor(node));
		assert(predecessor != nullptr);
		node_count_++;

		predecessor->left_ = node->left_;
		predecessor->right_ = node->right_;
		node_ref = predecessor;
		node->right_ = node->left_ = nullptr;
		return node;
	}

	Node*& findImpl(const key_t& key, Node*& node) {
		if (!node)
			return node;
		if (comparator_(key, node->key_))
			return findImpl(key, node->left_);
		else if (comparator_(node->key_, key))
			return findImpl(key, node->right_);

		else /*key == node->key_*/
			return node;
	}

	Node* findImplSplay(const key_t& key, Node*& node,
						EChildType& child_type, bool is_root) {
		if (!node)
			return nullptr;

		Node* result;
		EChildType grandchild = EChildType::UNDEFINED;

		if (comparator_(key, node->key_)) {
			node->visited();
			child_type = EChildType::LEFT;
			result = findImplSplay(key, node->left_, grandchild, false);
		} else if (comparator_(node->key_, key)) {
			node->visited();
			child_type = EChildType::RIGHT;
			result = findImplSplay(key, node->right_, grandchild, false);
		} else /*key == node->key_*/ {
			node->accessed();
			return node;
		}

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
		child->left_ = parent;
	}

	void rotateRight(Node*& parent_ref) {
		Node* parent = parent_ref;
		assert(parent != nullptr);
		Node* child = parent->left_;
		assert(child != nullptr);
		parent_ref = child;
		parent->left_ = child->right_;
		child->right_ = parent;
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
			pred = &(node->right_);
		return *pred;
	}

	Node* root_;
	size_t node_count_;
	const size_t max_node_count_;
	comparator_t comparator_;
};

#endif //NUMDB_SPLAY_TREE_BASE_H