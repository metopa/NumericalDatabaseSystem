/** @file binning_concurrent_adapter.h
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_BINNING_CONCURRENT_ADAPTER_H
#define NUMDB_BINNING_CONCURRENT_ADAPTER_H

#include <cstddef>
#include <experimental/optional>
#include <mutex>
#include <vector>
#include <random>
#include <memory>

#include <murmurhash2/all.h>

namespace numdb {
	namespace containers {
		template <typename KeyT, typename ValueT, typename ContainerTypeHolderT, size_t ThreadCount>
		class BinningConcurrentAdapter;

		template <typename ContainerTypeHolderT, size_t ThreadCount>
		struct BinningConcurrentAdapterTypeHolder {
			template <typename KeyT, typename ValueT>
			using container_t = BinningConcurrentAdapter<KeyT, ValueT, ContainerTypeHolderT, ThreadCount>;
		};

		template <typename KeyT, typename ValueT, typename ContainerTypeHolderT, size_t ThreadCount>
		class BinningConcurrentAdapter {
		  public:
			using inner_container_t = typename ContainerTypeHolderT::template container_t<KeyT, ValueT>;
			using lock_guard_t = std::lock_guard<std::mutex>;

			template <typename... Args>
			BinningConcurrentAdapter(size_t available_memory, size_t bins_count = ThreadCount,
									 Args... container_args) :
					locks_(bins_count) {
				if (bins_count == 0)
					throw std::invalid_argument("Binning container adapter: invalid bin count");
				hash_seed_ = std::random_device()();
				bins_.reserve(bins_count);
				for (size_t i = 0; i < bins_count; i++)
					bins_.emplace_back(new inner_container_t(available_memory / bins_count, container_args...));
			}

			size_t capacity() const {
				return bins_[0]->capacity() * bins_.size();
			}

			size_t size() const {
				size_t total = 0;
				for (auto& b : bins_)
					total += b.size();
				return total;
			}

			static constexpr bool isThreadsafe() {
				return true;
			}

			static constexpr size_t elementSize() {
				return inner_container_t::elementSize();
			}

			std::experimental::optional<ValueT> find(const KeyT& key) {
				size_t bin = getBin(key);
				lock_guard_t lock((locks_[bin]));
				return bins_[bin]->find(key);
			}

			void insert(KeyT key, ValueT value, size_t priority) {
				size_t bin = getBin(key);
				lock_guard_t lock((locks_[bin]));
				bins_[bin]->insert(std::move(key), std::move(value), priority);
			}

		  private:
			size_t getBin(const KeyT& key) const {
				return mmh2::getMurmurHash2(key, hash_seed_) % bins_.size();
			}

			size_t hash_seed_;

			std::vector<std::unique_ptr<inner_container_t>> bins_;
			std::vector<std::mutex> locks_;
		};
	}
}
#endif //NUMDB_BINNING_CONCURRENT_ADAPTER_H
