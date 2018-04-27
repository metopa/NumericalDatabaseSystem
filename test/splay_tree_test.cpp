#include <gtest/gtest.h>
#include <numdb/splay_tree/splay_tree_fair_lu.h>
#include <numdb/splay_tree/splay_tree_strategy.h>
#include <murmurhash2/functor.hpp>

/** @file splay_tree.cpp
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

using namespace numdb::containers;

#include "numdb/numdb.h"

TEST(SplayTree, basic) {
	using container_t = typename SplayTreeBottomNodeTypeHolder
			<CanonicalSplayStrategy>::template container_t<int, int>;
	container_t tree(container_t::elementSize() * 8);
	tree.insert(5, 5, 1);
	auto result = tree.find(5);
	ASSERT_EQ(bool(result), true);
	ASSERT_EQ(*result, 5);
	tree.remove(5);
	result = tree.find(5);
	ASSERT_EQ(bool(result), false);
}

TEST(SplayTree, many_insertions) {
	using container_t = typename SplayTreeBottomNodeTypeHolder
			<CanonicalSplayStrategy>::template container_t<size_t, int>;
	container_t tree(container_t::elementSize() * 100);
	double avg_height = 0;
	int reps = 10000;
	for (int i = 0; i < reps; i++) {
		tree.insert(mmh2::getMurmurHash2(i), i, 1);
		tree.verifyRefToSelfIntegrity();
		auto result = tree.find(mmh2::getMurmurHash2(i));
		ASSERT_EQ(bool(result), true);
		ASSERT_EQ(*result, i);
		tree.verifyRefToSelfIntegrity();
		avg_height += tree.computeTreeHeight();
	}
	//std::cout << "Average height after " << reps << " insertions and retrievals is " << avg_height / reps << std::endl;
}
