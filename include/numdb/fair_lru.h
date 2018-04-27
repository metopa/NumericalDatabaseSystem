/** @file fair_lru.h
 *  @brief
 *  @warning FairLRU does not manages a lifetime of it's nodes. Node class is designed to be a base class of another node, defined in a container and this contained must control a lifetime of it's nodes.
 *  @author Viacheslav Kroilov (metopa) <slavakroilov@gmail.com>
 */

#ifndef NUMDB_FAIR_LRU_H
#define NUMDB_FAIR_LRU_H

namespace numdb {
	namespace utility {
		class FairLRU {
		  public:
			struct Node {
				friend class FairLRU;

				Node() : next(nullptr), prev(nullptr) {}

			  protected:
				/**
				 * @remark Destructor is declared protected
				 * so only derived node can be deleted.
				 * With this limitation, dtor may not be declared virtual.
				 * This way we eliminate a vtable cost.
				*/
				~Node() = default;

			  private:
				Node* extract();
				void insertAfter(Node* node);

				Node* next;
				Node* prev;
			};

			FairLRU();
			FairLRU(const FairLRU&) = delete;
			FairLRU& operator =(const FairLRU&) = delete;

			Node* extractLuNode();
			void markRecentlyUsed(Node* node);
			void insertNode(Node* node);
			void extractNode(Node* node);

		  private:
			Node head_;
			Node tail_;
		};
	}
}
#endif //NUMDB_FAIR_LRU_H
