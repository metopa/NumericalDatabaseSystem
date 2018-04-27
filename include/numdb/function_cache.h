/** @file function_cache.h
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_FUNCTION_CACHE_H
#define NUMDB_FUNCTION_CACHE_H

#include <chrono>
#include <experimental/tuple>

#include <murmurhash2/all.h>
#include <function_traits/functional_unwrap.hpp>

#include "numdb/event_counter.h"
#include "numdb/utils.h"
#include "numdb/initial_priority_generator.h"

namespace numdb {
	template <
			typename UserFuncT,
			typename ContainerTypeHolder,
			typename EventCounterT = utility::EmptyEventCounter,
			typename UserFuncArgsTupleT = fu::argument_tuple_type_of_t<UserFuncT>
	>
	class FunctionCache {
	  public:
		using user_func_t = UserFuncT;
		using args_tuple_t = UserFuncArgsTupleT;
		using return_t =
		decltype(std::experimental::apply(
				std::declval<UserFuncT>(),
				std::declval<args_tuple_t>()));
		using event_counter_t = EventCounterT;
		using container_t = typename ContainerTypeHolder::template
		container_t<args_tuple_t, return_t>;


		template <typename... ContainerArgs>
		FunctionCache(UserFuncT user_func, size_t available_memory,
					  ContainerArgs&& ... container_args) :
				user_func_(std::move(user_func)),
				container_(available_memory,
						   std::forward<ContainerArgs>(container_args)...) {}

		size_t capacity() const {
			return container_.capacity();
		}

		size_t size() const {
			return container_.size();
		}

		static constexpr bool isThreadsafe() {
			return container_t::isThreadsafe();
		}

		size_t elementSize() const {
			return container_.elementSize();
		}

		double elementSizeOverhead() const {
			return 1 - (sizeof(args_tuple_t) + sizeof(return_t)) /
					   (double) elementSize();
		}

		EventCounterT& eventCounter() {
			return event_counter_;
		}

		template <typename... Args>
		auto operator ()(Args&& ... args) {
			static_assert(std::is_convertible<
								  std::tuple<Args...>, args_tuple_t>::value,
						  "Cannot convert provided arguments.");

			event_counter_.retrieve();

			args_tuple_t key(std::forward<Args>(args)...);

			auto res = container_.find(key);
			if (res) {
				return *res;
			}
			size_t priority = 0;
			auto computed_res = invokeUserFunc(key, priority);
			container_.insert(std::move(key), computed_res, priority);
			return std::move(computed_res);
		}

	  private:
		auto invokeUserFunc(const UserFuncArgsTupleT& args,
							size_t& priority) {
			using namespace std::chrono;
			event_counter_.invokeUserFunc();
			auto start = high_resolution_clock::now();
			DEFERRED(
					priority = priority_generator_.calculatePriority(
							(uint64_t) duration_cast<microseconds>(
									high_resolution_clock::now() - start
							).count());
			);
			return std::experimental::apply(user_func_, args);
		}

		container_t container_;
		utility::RatioPriorityGenerator<container_t::isThreadsafe(), 256> priority_generator_;
		UserFuncT user_func_;
		EventCounterT event_counter_;
	};
}
#endif //NUMDB_FUNCTION_CACHE_H
