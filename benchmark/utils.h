/** @file util.h
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_UTIL_H
#define NUMDB_UTIL_H

#include <boost/math/distributions/normal.hpp>

/**
 * @brief Compute sigma of a normal distribution (mean = 0) so that the range [-element_count / 2, element_count / 2] takes 'area' of area under the pdf curve
 * @param area
 * @param element_count
 * @return
 */

double computeSigma(double area, size_t element_count) {
	boost::math::normal N01;  // 'Standard' normal distribution with zero mean
	double q = boost::math::quantile(N01, 0.5 + area / 2);
	return element_count / 2 / q;
}


template <typename Result, typename... Args>
struct Fibonacci {
	Fibonacci(int fib_arg_min, int fib_arg_max) :
			fib_arg_min_(fib_arg_min), fib_arg_max_(fib_arg_max) {}

	Result operator ()(Args... args) {
		volatile int n = fib_arg_min_;
		if (fib_arg_min_ < fib_arg_max_)
			n += static_cast<int>(firstArg(args...)) % (fib_arg_max_ - fib_arg_min_ + 1);
		n = fibonacciImpl(n);
		return n;
	}

	static size_t aggregatedArgSize() {
		return sizeof(std::tuple<Result, Args...>);
	}
  private:
	const int fib_arg_min_;
	const int fib_arg_max_;
	int fibonacciImpl(int n) {
		return n <= 2 ? 1 : fibonacciImpl(n - 1) + fibonacciImpl(n - 2);
	}

	template <typename T, typename... Others>
	auto firstArg(T a, Others...) -> T {
		return a;
	};
};


size_t iterCnt(int area, size_t mem) {
	return mem * 400 * 100 / area + 1500;
}

double computeHitRate(size_t total_retrievals,
					  size_t user_func_calls,
					  double area_under_curve) {
	return (total_retrievals - user_func_calls) /
		   (double) total_retrievals * 100;
}

#endif //NUMDB_UTIL_H
