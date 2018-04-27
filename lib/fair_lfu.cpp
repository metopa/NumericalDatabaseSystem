/** @file fair_lfu.cpp
 *  @brief
 *
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#include "numdb/fair_lfu.h"

namespace numdb {
	namespace utility {
		FairLFU::FairLFU() {
			head_.group_next = &head_;
		}
		FairLFU::Node* FairLFU::extractLuNode() {
			assert(head_.next);
			Node* node = head_.next;
			extractNode(node);
			return node;
		}
		void FairLFU::markRecentlyUsed(FairLFU::Node* node) {
			assert(node);
			node->access_frequency++;
			if (node->group_next == node &&
				(!node->next ||
				 node->access_frequency < node->next->access_frequency))
				return;
			extractNode(node);
			if (node->group_next)
				insertAfter(node->group_next, node);
			else {
				Node* prev = node->prev;
				while (!prev->group_next) {
					assert(prev->prev);
					prev = prev->prev;
				}
				insertAfter(prev, node);
			}

		}
		void FairLFU::insertNode(FairLFU::Node* node) {
			Node* prev = head_.next ? head_.next : &head_;

			node->access_frequency = prev->access_frequency + 1;
			insertAfter(prev, node);
		}
		void FairLFU::extractNode(FairLFU::Node* node) {
			assert(node != nullptr);
			if (node->inFreqList()) {
				if (node->group_next == node) {
					if (node->prev) {
						assert(node->prev->next == node);
						assert(node->prev->access_frequency < node->access_frequency);
						node->prev->next = node->next;
					}
					if (node->next) {
						assert(node->next->prev == node);
						node->next->prev = node->prev;
					}
				} else {
					// promote next node in group as a group head
					Node* new_head = node->group_next;
					new_head->group_next = new_head->next ? new_head->next : new_head;
					new_head->prev = node->prev;
					new_head->next = node->next;
					if (node->prev) {
						assert(node->prev->next == node);
						node->prev->next = new_head;
					}
					if (node->next) {
						assert(node->next->prev == node);
						node->next->prev = new_head;
					}
				}
			} else { //Node is in a group list
				assert(node->prev != nullptr);
				if (node->prev->inFreqList())
					node->prev->group_next = node->next ? node->next : node->prev;
					// freq list node points to itself
					// when is a single node in the group
				else {
					assert(node->prev->next == node);
					node->prev->next = node->next;
				}
				if (node->next) {
					assert(node->next->prev == node);
					node->next->prev = node->prev;
				}
			}
		}
		void FairLFU::insertAfter(FairLFU::Node* prev, FairLFU::Node* node) {
			assert(prev);
			assert(prev->inFreqList());
			assert(node);
			assert(prev->access_frequency <= node->access_frequency);
			if (prev->access_frequency == node->access_frequency) {
				node->group_next = nullptr;
				node->prev = prev;
				node->next = prev->group_next == prev ? nullptr : prev->group_next;
				prev->group_next = node;
				if (node->next)
					node->next->prev = node;
			} else if (!prev->next ||
					   prev->next->access_frequency > node->access_frequency) {
				node->group_next = node;
				node->next = prev->next;
				node->prev = prev;
				prev->next = node;
				if (node->next) {
					assert(node->next->prev == prev);
					node->next->prev = node;
				}
			} else
				insertAfter(prev->next, node);
		}

		bool FairLFU::Node::inFreqList() const {
			return group_next != nullptr;
		}
	}
}
