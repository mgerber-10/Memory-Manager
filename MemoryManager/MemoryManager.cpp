#include "MemoryManager.h"

/*-------------------------------------------|
|  Memory Allocation Algorithm Definitions   |
|-------------------------------------------*/

/* BEST FIT ALGORITHM
- Returns word offset of hole selected by the best fit algorithm, returns -1 if no fit     
*/
int bestFit(int sizeInWords, void *list)
{
    int offset = -1;
    int max = 65536;
    uint16_t *holeList = static_cast<uint16_t *>(list);
    uint16_t holeListLength = *holeList++;

    for (uint16_t i = 1; i < (holeListLength)*2; i += 2)
    {
        if (holeList[i] >= sizeInWords && holeList[i] < max)
        {
            offset = holeList[i - 1];
            max = holeList[i];
        }
    }
    return offset;
}

/* WORST FIT ALGORITHM
- Returns word offset of hole selected by the worst fit algorithm, returns -1 if no fit    
*/
int worstFit(int sizeInWords, void *list)
{
    int offset = -1;
    int min = -1;
    uint16_t *holeList = static_cast<uint16_t *>(list);
    uint16_t holeListLength = *holeList++;

    for (uint16_t i = 1; i < (holeListLength)*2; i += 2)
    {
        if (holeList[i] >= sizeInWords && holeList[i] > min)
        {
            offset = holeList[i - 1];
            min = holeList[i];
        }
    }
    return offset;
}


/*--------------------------------------------|
|      MemoryManager Class Definitions        |
|--------------------------------------------*/

/* CONTSTRUCTOR:
Sets naive word size (in bytes, for alignment) and default allocator for finding a memory hole
*/
MemoryManager::MemoryManager(unsigned wordSize, std::function<int(int, void *)> allocator) 
{
    this->algorithmType = allocator;    // algorithm type used
    this->wordSize = wordSize;          // # of words
}

/* DESTRUCTOR:
Releases all memory allocated by this object WITHOUT LEAKING MEMORY!!
*/
MemoryManager::~MemoryManager()
{
    // Call the release (shutdown) function to empty the vectors representing the currently allocated memory
    shutdown();
}

/* INITIALIZER:
Instantiates a block of requested size, no larger than 65536 words, cleans up previous block if applicable
*/
void MemoryManager::initialize(size_t sizeInWords)
{
    // Initially, check requirement that requested block size is no larger than 65536 words
    if (sizeInWords <= 65536)
    {
        memLimit = wordSize * sizeInWords;                    // calculate memory limit (# of bytes available) by multiplying # of words & bytes per word
        memoryBlock = std::vector<uint64_t>(memLimit);        // allocate a memoryBlock of that many bytes
        start = (uint64_t *)memoryBlock.data();               // set start to be the address of first element in memoryBlock
        indexToNodeMap[0] = 0;                                // initialize mapping to 0   
        listNodes.push_back(listNode(0, sizeInWords, true));  // add node to the listNodes vector with start(0),  blockSize(sizeInWords) & isHole(true)
    }
}

/* RELEASER:
Releases memory block acquired during initialization, if any. Does this WITHOUT LEAKING MEMORY
*/
void MemoryManager::shutdown()
{
    // Empty the respective vectors representing the memoryBlock, the list of nodes, and the map of nodes to locations in memory
    listNodes.clear();
    memoryBlock.clear();
    indexToNodeMap.clear();
}

/* ALLOCATOR:
Allocates a memory using the allocator function; If no memory available or invalid size, returns nullptr
*/
void *MemoryManager::allocate(size_t sizeInBytes)
{
    size_t sizeInWords = ceil((float)sizeInBytes / wordSize);           // Gets the sizeInWords from dividing sizeInBytes by length of words
    uint16_t *newListPtr = static_cast<uint16_t *>(getList());          // Creates pointer to list created from getList(), which memory with 'new'
    int availableHole = algorithmType(sizeInWords, newListPtr);         // Uses specified algorithm to find a hole in memory suitable for allocation
    delete[] newListPtr;                                                // 'delete' allocated memory from getList (PREVENTS MEMORY LEAKS)

    // Return nullptr, if failed to find availableHole or invalid size
    if (availableHole == -1 || indexToNodeMap.count(availableHole) == 0)
    {
        return nullptr;
    }

    int nodeIndex = indexToNodeMap[availableHole];

    // if there is more space, allocate and split listNode
    if (listNodes[nodeIndex].size != sizeInWords)
    {
        splitNode(nodeIndex, availableHole, sizeInWords);
    }
    else
    {
        listNodes[nodeIndex].isHole = false;
    }
    return (void *)(memoryBlock.data() + availableHole);                // Return pointer to allocated memoryBlock
}

// Helper allocate function: splits listNode in linked list of allocated memory blocks, creating new node to represent the allocated block & updates as needed
void MemoryManager::splitNode(int nodeIndex, int availableHole, size_t sizeInWords)
{
    // Create listNode representing allocated memory block
    listNode usedNode = listNode(listNodes[nodeIndex].headIndex, sizeInWords, false);
    listNodes.insert(listNodes.begin() + nodeIndex, usedNode);

    // Update previous listNodes
    listNodes[nodeIndex + 1].headIndex += sizeInWords;
    listNodes[nodeIndex + 1].size -= sizeInWords;

    // Update map
    indexToNodeMap[listNodes[nodeIndex].headIndex] = nodeIndex;
    indexToNodeMap[listNodes[nodeIndex + 1].headIndex] = nodeIndex + 1;

    //  Iterate over indexToNodeMap to update indices of all nodes after the newly inserted node (to represent correct position in the linked list after the insertion)
    for (auto iter = indexToNodeMap.find(listNodes[nodeIndex + 1].headIndex); iter != indexToNodeMap.end(); iter++)
    {
        if (iter == indexToNodeMap.find(listNodes[nodeIndex + 1].headIndex))
        {
            // Ignore first item
            continue;
        }
        else
        {
            iter->second++;
        }
    }
}

/* DEALLOCATOR:
Frees the memory block within the memory manager so it can be reused
*/
void MemoryManager::free(void *address)
{
    // Find node and make it a hole
    int bytePosition = (uint64_t *)address - memoryBlock.data();

    // If no associated hole, return
    if (indexToNodeMap.count(bytePosition) == 0)
    {
        return;
    }

    // Create nodeIndex corresponding to index in map from byteIndex
    int nodePosition = indexToNodeMap[bytePosition];
    listNodes[nodePosition].isHole = true;

    // combine holes if necessary
    mergeHoles(nodePosition);
}

// Helper deallocate function, merges adjacent holes in memory
void MemoryManager:: mergeHoles(int nodePosition)
{
    if (nodePosition > 0 && listNodes[nodePosition - 1].isHole)
    {
        // If prev is a hole, merge holes
        indexToNodeMap.erase(listNodes[nodePosition].headIndex);
        listNodes[nodePosition - 1].size += listNodes[nodePosition].size;     // merge holes with prev
        listNodes.erase(listNodes.begin() + nodePosition);                    // remove current

        nodePosition--;    // decrement in case next is also a hole
    }
    else if (listNodes.size() > nodePosition && listNodes[nodePosition + 1].isHole)
    {
        // Else if next is a hole, merge holes
        for (auto iter = indexToNodeMap.find(listNodes[nodePosition + 1].headIndex); iter != indexToNodeMap.end(); iter++)
        {
            iter->second--;
        }
        indexToNodeMap.erase(listNodes[nodePosition + 1].headIndex);
        listNodes[nodePosition].size += listNodes[nodePosition + 1].size;     // merge holes with next
        listNodes.erase(listNodes.begin() + nodePosition + 1);                // remove current
    }
}

/* SET ALGORTIHM TYPE:
Changes the allocator algorithm of identifying the memory hole to use for allocation       
*/
void MemoryManager::setAllocator(std::function<int(int, void *)> allocator)
{
    this->algorithmType = allocator;
}

/* ADD HOLE LIST TO FILENAME:
Uses standard POSIX calls to write hole list to filename AS TEXT, returns -1 on error & 0 if successful
*/
int MemoryManager::dumpMemoryMap(char *filename)
{
    // use POSIX calls to write holeList to file
    int file = open(filename, O_CREAT | O_WRONLY, 0600);
    if (file < 0)
    {
        return -1;
    }

    std::string outputStr = "";
    uint16_t *holeList = static_cast<uint16_t *>(getList());   // call to getList, allocates memory with 'new' 
    uint16_t *listEntryPoint = holeList;
    uint16_t holeListlength = *holeList++;

    // For each pair of data in holeList add them to outputStr and surround them in brackets
    for (uint16_t i = 0; i < (holeListlength)*2; i += 2)
    {
        outputStr += "[" + std::to_string(holeList[i]) + ", " + std::to_string(holeList[i + 1]) + "] - ";
    }

    outputStr = outputStr.substr(0, outputStr.size() - 3);

    const char *c = outputStr.c_str();
    delete[] listEntryPoint;            // Delete memory previously allocated in getList (PREVENTS LEAKS)

    // Return -1, if write operation or close operation fail
    if (write(file, c, strlen(c)) == -1 || close(file) == -1)
    {
        return -1;
    }
    else 
    {
        // Return 0, if successful
        return 0;
    }
}

/* GET LIST:
Returns an array of information (in decimal) about holes for use by the allocator function, if no memory has been allocated, returns nullptr
*/
void *MemoryManager::getList()
{
    // Return nullptr, if listNodes is empty (no memory allocated)
    if (listNodes.size() == 0)
    {
        return nullptr;
    }
    
    // Create vector representing list of holes in memory
    std::vector<int> holeList;
    int holeCount = 0;

    // For each item in list nodes, if its a hole add it to holeList
    for (unsigned int i = 0; i < listNodes.size(); i++)
    {
        if (listNodes[i].isHole)
        {
            holeCount++;
            holeList.push_back(listNodes[i].headIndex);
            holeList.push_back(listNodes[i].size);
        }
    }
    holeList.insert(holeList.begin(), holeCount);
    uint16_t *list = new uint16_t[1 + (holeCount * 2)];             // Allocate memory for 'list' which points to an array containing the holes in memory

    // For each hole in hole list add it to newly allocated list
    for (int i = 0; i < holeList.size(); i++)
    {
        list[i] = (uint16_t)holeList[i];
    }
    return list;
}

/* GET BIT MAP:
Returns a bit-stream in terms of an array representing whether words are used (1) or free (0)
First 2 bytes are size of bitmap, rest is the bitmap
*/
void *MemoryManager::getBitmap()
{
    std::string binaryStr = "";
    std::vector<std::string> binaryVec;

    // For each node in listNodes, set binary value based on if its a hole or is used
    for (auto currNode : listNodes)
    {
        int listLength = currNode.size;
        while (listLength > 0)
        {
            if (currNode.isHole)
            {
                binaryStr = "0" + binaryStr;
            }
            else
            {
                binaryStr = "1" + binaryStr;
            }
            listLength--;
        }
    }
    
    int sizeMap = (binaryStr.size() + 8 - 1) / 8;

    // While binary string isnt empty add 8-bit string to binaryVec
    while (binaryStr.size() > 0)
    {
        if (binaryStr.size() > 8)
        {
            binaryVec.push_back(binaryStr.substr(binaryStr.size() - 8));
            binaryStr = binaryStr.substr(0, binaryStr.size() - 8);
        }
        else
        {
            binaryVec.push_back(binaryStr);
            binaryStr.clear();
        }
    }

    std::string binSize = std::bitset<8>(sizeMap).to_string();
    binaryVec.insert(binaryVec.begin(), binSize.substr(4));
    binaryVec.insert(binaryVec.begin() + 1, binSize.substr(0, 4));
    uint8_t *list = new uint8_t[binaryVec.size()];                      // Allocates 'new' array of 8-bit integers

    // for each item in binaryVec convert it to an unsigned long int and store it in list
    for (int i = 0; i < binaryVec.size(); i++)
    {
        list[i] = (uint8_t)std::bitset<8>(binaryVec[i]).to_ullong();
    }
    return list;
}

/* GET WORD SIZE:
Returns word size used for alignment
*/
unsigned MemoryManager::getWordSize()
{
    return wordSize;
}

/* GET MEMORY START:
Returns the byte-wise memory address of the beginning of the memory block
*/
void *MemoryManager::getMemoryStart()
{
    return start;
}

/* GET MEMORY LIMIT:
Returns the byte limit of the current memory block
*/
unsigned MemoryManager::getMemoryLimit()
{
    return memLimit;
}
