#pragma once

#include <string>
#include <cstring>
#include <vector>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <bitset>
#include <cmath>
#include <fcntl.h>
#include <unistd.h>

/*
|--------------------------------------------------------------------------------|
|  Linked List Node Struct                                                       |
|     - Contains int representing number of words, int representing headIndex    |
|       and a bool representing if the node is a hole (isFree) or is allocated   |
|--------------------------------------------------------------------------------|
*/
struct listNode
{
        int size;                       // number of words
        int headIndex;                  // position in memory
        bool isHole = true;             // is space free or occupied?

        listNode(int headIndex, int size, bool isHole)
        {
            this->headIndex = headIndex;
        	this->size = size;
       		this->isHole = isHole;
        }
};


/*
|--------------------------------------------------------------------------------|
|  MemoryManager Class Declaration                                               |
|   - Handles allocation/deallocation of memory & provides details of its state  |
|--------------------------------------------------------------------------------|
*/
class MemoryManager
{
 private:
        std::vector<listNode> listNodes;              // Vector representing nodes in memory
        std::vector<uint64_t> memoryBlock;            // Vector representing a memory block

        std::map<int, int> indexToNodeMap;            // mapping of listNodes to locations in memory

        uint64_t* start;                              // pointer to start of memory block
        unsigned wordSize;                            // value representing length of words
        size_t memLimit;                              // value representing number of bytes available from start

        std::function<int(int, void*)> algorithmType; // allocator algorithm used for determining which holes to use

 public:

        // Constructor / Destructor
        MemoryManager(unsigned wordSize, std::function<int(int, void*)> allocator);        
        ~MemoryManager();

        // Initialize / Release Memory Block
        void initialize(size_t sizeInWords);
        void shutdown();

        // Allocate / Deallocate Sections Of Memory
        void* allocate(size_t sizeInBytes);
        void free(void* address);

        // Set Functions (Mutators)
        void setAllocator(std::function<int(int, void*)> allocator);
        int dumpMemoryMap(char* filename);

        // Get Functions (Accessors)
        void* getList();
        void* getBitmap();
        unsigned getWordSize();
        void* getMemoryStart();
        unsigned getMemoryLimit();

		// Helper functions
		void splitNode(int nodePosition, int availableHole, size_t sizeInWords);
		void mergeHoles(int nodePosition);
};


/*
|-------------------------------------------------------------------|
|  Memory Allocation Algorithm Declarations                         |
|   - Algorithms to find holes in memory to allocate to a process   |
|-------------------------------------------------------------------|
*/
int bestFit(int sizeInWords, void* list);
int worstFit(int sizeInWords, void* list);
