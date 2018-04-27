/** @file fair_lfu.h
 *  @brief
 *  @warning FairLFU does not manages a lifetime of it's nodes. Node class is designed to be a base class of another node, defined in a container and this contained must control a lifetime of it's nodes.
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_FAIR_LFU_H
#define NUMDB_FAIR_LFU_H

#include <cstdint>
#include <cassert>

namespace numdb {
	namespace utility {
		class FairLFU {
		  public:
			struct Node {
				friend class FairLFU;

			  protected:
				/**
				 * @remark Destructor is declared protected so only derived node can be deleted. With this limitation, dtor may not be declared virtual. This way we eliminate a vtable cost.
				*/
				~Node() = default;

			  private:
				bool inFreqList() const;

				uint64_t access_frequency = 0;
				Node* next = nullptr;
				Node* prev = nullptr;
				Node* group_next = nullptr;
			};

			FairLFU();
			FairLFU(const FairLFU&) = delete;
			FairLFU& operator =(const FairLFU&) = delete;

			Node* extractLuNode();

			void markRecentlyUsed(Node* node);

			void insertNode(Node* node);

			void extractNode(Node* node);

		  private:
			void insertAfter(Node* prev, Node* node);

			Node head_;
		};


	}
}
#endif //NUMDB_FAIR_LFU_H
