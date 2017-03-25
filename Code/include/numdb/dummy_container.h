/** @file dummy_container.h
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_DUMMY_CONTAINER_H
#define NUMDB_DUMMY_CONTAINER_H


#include <cstddef>
#include <experimental/optional>

template <typename KeyT, typename ValueT>
class DummyContainer;

struct DummyContainerTypeHolder {
	template <typename KeyT, typename ValueT>
	using container_t = DummyContainer<KeyT, ValueT>;
};

template <typename KeyT, typename ValueT>
class DummyContainer {
  public:
	DummyContainer(size_t available_memory) {}

	size_t capacity() const {
		return 0;
	}

	size_t size() const {
		return 0;
	}

	static constexpr bool isThreadsafe() {
		return true;
	}

	size_t elementSize() const {
		return sizeof(KeyT) + sizeof(ValueT);
	}

	std::experimental::optional<ValueT> find(const KeyT& key) {
		return {};
	}

	void insert(const KeyT& key, const ValueT& value) {}
};


#endif //NUMDB_DUMMY_CONTAINER_H
