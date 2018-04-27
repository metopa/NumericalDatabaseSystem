/** @file bench.cpp
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#include <random>
#include <memory>
#include <benchmark/benchmark.h>
#include <iostream>
#include "utils.h"

#include "numdb/numdb.h"

#define PARALLEL_ONLY 0

using namespace numdb;
using containers::CanonicalSplayStrategy;
using containers::WstSplayStrategy;
using containers::AccessCountSplayStrategy;
using containers::ParametrizedAccessCountSplayStrategy;

//FIXME compute sigma parameter basing on the function arity
/**
 * @arg state.range(0) min argument for a Fibonacci function
 * @arg state.range(1) max argument for a Fibonacci function
 * @arg state.range(2) available memory (in KiB)
 * @arg state.range(3) desired area under curve (in percents)
 * @arg state.range(4) distribution slide speed * 100
 */
template <class ContainerTypeHolder>
void BM(benchmark::State& state) {
	using UserFunc = Fibonacci<int, double>;

	size_t mem = static_cast<size_t>(state.range(2)) * 1024;
	size_t max_element_capacity = mem / UserFunc::aggregatedArgSize();

	double area = state.range(3);
	area /= 100;

	double mean_delta = state.range(4) / 100.;
	double mean_offset = 0;

	double sigma = computeSigma(area, max_element_capacity);
	std::random_device r;
	std::mt19937 e(r());
	std::normal_distribution<double> dist(0, sigma);

	FunctionCache<UserFunc, ContainerTypeHolder, utility::AtomicEventCounter>
			cache(UserFunc(state.range(0), state.range(1)), mem);


	while (state.KeepRunning()) {
		double a = std::round(dist(e) + mean_offset);
		benchmark::DoNotOptimize(cache(a));
		mean_offset += mean_delta;
	}

	state.SetItemsProcessed(state.iterations());
	state.counters["cap"] = cache.capacity();
	state.counters["hr"] = computeHitRate(cache.eventCounter().total_retrievals,
										  cache.eventCounter().user_func_invocations, area);
}

/**
 * @arg state.range(0) min argument for a Fibonacci function
 * @arg state.range(1) max argument for a Fibonacci function
 * @arg state.range(2) available memory (in KiB)
 * @arg state.range(3) desired area under curve (in percents)
 * @arg state.range(4) distribution slide speed * 100
 */
template <class ContainerTypeHolder, size_t... ContainerArgs>
void ParallelBM(benchmark::State& state) {
	using UserFunc = Fibonacci<int, double>;

	size_t mem = static_cast<size_t>(state.range(2)) * 1024;
	size_t max_element_capacity = mem / UserFunc::aggregatedArgSize();

	double area = state.range(3);
	area /= 100;

	double mean_delta = state.range(4) / 100.;
	double mean_offset = 0;

	double sigma = computeSigma(area, max_element_capacity);
	std::random_device r;
	std::mt19937 e(r());
	std::normal_distribution<double> dist(0, sigma);


	using cache_t = FunctionCache<UserFunc, ContainerTypeHolder, utility::AtomicEventCounter>;
	static std::unique_ptr<cache_t> cache_ptr;

	if (state.thread_index == 0)
		cache_ptr.reset(new cache_t(UserFunc(state.range(0), state.range(1)), mem, ContainerArgs...));

	while (state.KeepRunning()) {
		double a = std::round(dist(e) + mean_offset);
		benchmark::DoNotOptimize((*cache_ptr)(a));
		mean_offset += mean_delta;
		//if (state.iterations() % 1000 == 0) std::cerr << state.thread_index << '|';
	}

	state.SetItemsProcessed(state.iterations());

	if (state.thread_index == 0) {
		state.counters["cap"] = cache_ptr->capacity();
		state.counters["hr"] = computeHitRate(cache_ptr->eventCounter().total_retrievals,
											  cache_ptr->eventCounter().user_func_invocations, area);
		cache_ptr.reset();
		std::cerr << std::endl;
	}
}

constexpr int maxThreads = 16;

void memSequence(benchmark::internal::Benchmark* b) {
	int min_memory = 4;
	int max_memory = 64;
	constexpr int mem_multiply = 4;

	int arg_min = 25;
	int arg_max = 35;

	int default_area = 85;

	int default_momentum = 1;

	b->ArgNames({"fib_min", "fib_max", "memory", "area", "mean_momentum"});

	for (int mem = min_memory; mem <= max_memory; mem *= mem_multiply)
		b->Iterations(iterCnt(default_area, mem))
				->Args({arg_min, arg_max, mem,
						default_area, 0});
	for (int mem = min_memory; mem <= max_memory; mem *= mem_multiply)
		b->Iterations(iterCnt(default_area, mem))
				->Args({arg_min, arg_max, mem,
						default_area, default_momentum});

}

void memSequencePlus(benchmark::internal::Benchmark* b) {
	int min_memory = 256;
	int max_memory = 1024 * 4;
	constexpr int mem_multiply = 4;

	int arg_min = 25;
	int arg_max = 35;

	int default_area = 85;
	int default_momentum = 1;


	b->ArgNames({"fib_min", "fib_max", "memory", "area", "mean_momentum"});

	for (int mem = min_memory; mem <= max_memory; mem *= mem_multiply)
		b->Iterations(iterCnt(default_area, mem) / 2)
				->Args({arg_min, arg_max, mem,
						default_area, 0});
	for (int mem = min_memory; mem <= max_memory; mem *= mem_multiply)
		b->Iterations(iterCnt(default_area, mem) / 2)
				->Args({arg_min, arg_max, mem,
						default_area, default_momentum});
}

void areaSequence(benchmark::internal::Benchmark* b) {
	int default_memory = 64;

	int arg_min = 25;
	int arg_max = 35;
	int default_momentum = 1;

	for (int area = 45; area <= 95; area += 10)
		b->Iterations(iterCnt(area, default_memory))
				->Args({arg_min, arg_max, default_memory,
						area, default_momentum});
}

void momentumSequence(benchmark::internal::Benchmark* b) {
	int default_memory = 64;
	int arg_min = 25;
	int arg_max = 35;
	int default_area = 85;

	b->ArgNames({"fib_min", "fib_max", "memory", "area", "mean_momentum"});

	b->Iterations(iterCnt(default_area, default_memory))
			->Args({arg_min, arg_max, default_memory,
					default_area, 0});
	for (int momentum = 1; momentum <= 1000; momentum *= 10)
		b->Iterations(iterCnt(default_area, default_memory))
				->Args({arg_min, arg_max, default_memory,
						default_area, momentum});
}

void fullSequence(benchmark::internal::Benchmark* b) {
	memSequence(b);
	areaSequence(b);
	momentumSequence(b);
}

BENCHMARK_TEMPLATE(BM, DummyContainer)->Iterations(1000)->Args({25, 35, 100, 50, 0});

#if !PARALLEL_ONLY
BENCHMARK_TEMPLATE(BM, WeightedSearchTree<0>)->Apply(fullSequence);
BENCHMARK_TEMPLATE(BM, WeightedSearchTree<1>)->Apply(fullSequence)->Apply(memSequencePlus);
BENCHMARK_TEMPLATE(BM, WeightedSearchTree<2>)->Apply(fullSequence);
BENCHMARK_TEMPLATE(BM, WeightedSearchTree<4>)->Apply(fullSequence);
BENCHMARK_TEMPLATE(BM, WeightedSearchTree<16>)->Apply(fullSequence);


BENCHMARK_TEMPLATE(BM, PriorityHashtable<0>)->Apply(fullSequence);
BENCHMARK_TEMPLATE(BM, PriorityHashtable<1>)->Apply(fullSequence)->Apply(memSequencePlus);;
BENCHMARK_TEMPLATE(BM, PriorityHashtable<2>)->Apply(fullSequence);
BENCHMARK_TEMPLATE(BM, PriorityHashtable<4>)->Apply(fullSequence);
BENCHMARK_TEMPLATE(BM, PriorityHashtable<16>)->Apply(fullSequence);
BENCHMARK_TEMPLATE(BM, LruHashtable<>)->Apply(fullSequence);
BENCHMARK_TEMPLATE(BM, LfuHashtable<>)->Apply(fullSequence);

BENCHMARK_TEMPLATE(BM, BottomNodeSplayTree<CanonicalSplayStrategy>)->Apply(fullSequence)->Apply(memSequencePlus);;
BENCHMARK_TEMPLATE(BM, BottomNodeSplayTree<AccessCountSplayStrategy>)->Apply(fullSequence);
BENCHMARK_TEMPLATE(BM, BottomNodeSplayTree<WstSplayStrategy<0>>)->Apply(fullSequence);
BENCHMARK_TEMPLATE(BM, BottomNodeSplayTree<WstSplayStrategy<1>>)->Apply(fullSequence);
BENCHMARK_TEMPLATE(BM, BottomNodeSplayTree<WstSplayStrategy<4>>)->Apply(fullSequence);

BENCHMARK_TEMPLATE(BM, LruSplayTree<CanonicalSplayStrategy>)->Apply(fullSequence);
BENCHMARK_TEMPLATE(BM, LfuSplayTree<CanonicalSplayStrategy>)->Apply(fullSequence);

#endif

BENCHMARK_TEMPLATE(ParallelBM, CoarseLockAdapter<WeightedSearchTree<1>>)
->ThreadRange(1, maxThreads)->Apply(fullSequence);

BENCHMARK_TEMPLATE(ParallelBM, BinningConcurrentAdapter<WeightedSearchTree<0>, maxThreads>)
->ThreadRange(1, maxThreads)->Apply(fullSequence);

BENCHMARK_TEMPLATE(ParallelBM, CNDC<false>)
->ThreadRange(1, maxThreads)->Apply(fullSequence);
BENCHMARK_TEMPLATE(ParallelBM, CNDC<true>)
->ThreadRange(1, maxThreads)->Apply(fullSequence);

BENCHMARK_MAIN();
