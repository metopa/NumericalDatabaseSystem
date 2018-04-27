/** @file fair_lru.cpp
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#include "numdb/fair_lru.h"

namespace numdb {
	namespace utility {
		FairLRU::Node* FairLRU::Node::extract() {
			prev->next = next;
			if (next)
				next->prev = prev;
			return this;
		}
		void FairLRU::Node::insertAfter(FairLRU::Node* node) {
			prev = node;
			next = node->next;
			node->next = this;
			if (next)
				next->prev = this;
		}
		FairLRU::FairLRU() {
			tail_.insertAfter(&head_);
		}
		FairLRU::Node* FairLRU::extractLuNode() {
			if (head_.next == &tail_)
				return nullptr;
			return head_.next->extract();
		}
		void FairLRU::markRecentlyUsed(FairLRU::Node* node) {
			node->extract();
			insertNode(node);
		}
		void FairLRU::insertNode(FairLRU::Node* node) {
			node->insertAfter(tail_.prev);
		}
		void FairLRU::extractNode(FairLRU::Node* node) {
			node->extract();
		}
	}
}
