/** @file coarse_lock_concurrent_adapter.h
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_COARSE_LOCK_CONCURRENT_ADAPTER_H
#define NUMDB_COARSE_LOCK_CONCURRENT_ADAPTER_H

#include <cstddef>
#include <mutex>
#include <experimental/optional>


namespace numdb {
	namespace containers {

		template <typename KeyT, typename ValueT, typename ContainerTypeHolderT>
		class CoarseLockConcurrentAdapter;

		template <typename ContainerTypeHolderT>
		struct CoarseLockConcurrentAdapterTypeHolder {
			template <typename KeyT, typename ValueT>
			using container_t = CoarseLockConcurrentAdapter<KeyT, ValueT, ContainerTypeHolderT>;
		};

		template <typename KeyT, typename ValueT, typename ContainerTypeHolderT>
		class CoarseLockConcurrentAdapter {
		  public:
			using inner_container_t = typename ContainerTypeHolderT::template container_t<KeyT, ValueT>;
			using lock_guard_t = std::lock_guard<std::mutex>;
			template <typename... Args>
			CoarseLockConcurrentAdapter(Args... container_args) :
					inner_container(container_args...) {}

			size_t capacity() const {
				return inner_container.capacity();
			}

			size_t size() const {
				return inner_container.size();
			}

			static constexpr bool isThreadsafe() {
				return true;
			}

			static constexpr size_t elementSize() {
				return inner_container_t::elementSize();
			}

			std::experimental::optional<ValueT> find(const KeyT& key) {
				lock_guard_t lg(mutex_);
				return inner_container.find(key);
			}

			void insert(const KeyT& key, const ValueT& value, size_t priority) {
				lock_guard_t lg(mutex_);
				inner_container.insert(key, value, priority);
			}

		  private:
			inner_container_t inner_container;
			std::mutex mutex_;
		};
	}
}

#endif //NUMDB_COARSE_LOCK_CONCURRENT_ADAPTER_H
