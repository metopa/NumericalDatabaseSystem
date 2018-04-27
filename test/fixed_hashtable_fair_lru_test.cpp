/** @file fixed_hashtable_fair_lru_test.cpp
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */


#include <gtest/gtest.h>
#include "numdb/numdb.h"

using namespace numdb::containers;
using namespace numdb::utility;

TEST(HashTable_FairLRU, basic) {
	using container_t = typename FixedHashtableFairLeastUsedTypeHolder<FairLRU>::template container_t<int, std::string>;
	container_t ht(20 * container_t::elementSize(0.5), 0.5);
	ht.insert(10, "AAA", 1);
	auto result = ht.find(10);
	ASSERT_EQ(bool(result), true);
	ASSERT_EQ(*result, "AAA");
	result = ht.find(30);
	ASSERT_EQ(bool(result), false);
}

TEST(HashTable_FairLRU, tuple_key) {
	using container_t = typename FixedHashtableFairLeastUsedTypeHolder<FairLRU>::template container_t<
			std::tuple<double, double, double>,
			std::string>;
	container_t ht(20 * container_t::elementSize(0.5), 0.5);

	ht.insert(std::make_tuple(0., 0., 0.), "000", 1);
	ht.insert(std::make_tuple(1., 0., 0.), "100", 1);
	ht.insert(std::make_tuple(0., 1., 0.), "010", 1);
	ht.insert(std::make_tuple(0., 0., 1.), "001", 1);
	auto result = ht.find(std::make_tuple(-0., -0., -0.));
	ASSERT_EQ(bool(result), true);
	ASSERT_EQ(*result, "000");

	result = ht.find(std::make_tuple(1., 0., 0.));
	ASSERT_EQ(bool(result), true);
	ASSERT_EQ(*result, "100");

	result = ht.find(std::make_tuple(0., 1, 0.));
	ASSERT_EQ(bool(result), true);
	ASSERT_EQ(*result, "010");

	result = ht.find(std::make_tuple(0., 0., 1));
	ASSERT_EQ(bool(result), true);
	ASSERT_EQ(*result, "001");

	result = ht.find(std::make_tuple(0., -1, 0.));
	ASSERT_EQ(bool(result), false);
}

TEST(HashTable_FairLRU, limit_element_count_1) {
	using container_t = typename FixedHashtableFairLeastUsedTypeHolder<FairLRU>::template container_t<int, int>;
	container_t ht(10 * container_t::elementSize(0.5), 0.5);
	for (int i = 10; i < 25; i++)
		ht.insert(i, i, 1);

	for (int i = 10; i < 15; i++) {
		auto result = ht.find(i);
		ASSERT_EQ(bool(result), false);
	}

	for (int i = 15; i < 25; i++) {
		auto result = ht.find(i);
		ASSERT_EQ(bool(result), true);
		ASSERT_EQ(*result, i);
	}
}

TEST(HashTable_FairLRU, lru_test) {
	using container_t = typename FixedHashtableFairLeastUsedTypeHolder<FairLRU>::template container_t<int, int>;
	container_t ht(10 * container_t::elementSize(0.5), 0.5);
	for (int i = 10; i < 25; i++)
		ht.insert(i, i, 1);

	for (int i = 10; i < 15; i++) {
		auto result = ht.find(i);
		ASSERT_EQ(bool(result), false);
	}

	for (int i = 15; i < 20; i++) {
		auto result = ht.find(i);
		ASSERT_EQ(bool(result), true);
		ASSERT_EQ(*result, i);
	}

	for (int i = 10; i < 15; i++)
		ht.insert(i, i, 1);

	for (int i = 14; i >= 10; i--) {
		auto result = ht.find(i);
		ASSERT_EQ(bool(result), true);
		ASSERT_EQ(*result, i);
	}

	for (int i = 30; i < 35; i++)
		ht.insert(i, i, 1);

	for (int i = 10; i < 15; i++) {
		auto result = ht.find(i);
		ASSERT_EQ(bool(result), true);
		ASSERT_EQ(*result, i);
	}

	for (int i = 15; i < 30; i++) {
		auto result = ht.find(i);
		ASSERT_EQ(bool(result), false);
	}

	for (int i = 30; i < 35; i++) {
		auto result = ht.find(i);
		ASSERT_EQ(bool(result), true);
		ASSERT_EQ(*result, i);
	}
}
