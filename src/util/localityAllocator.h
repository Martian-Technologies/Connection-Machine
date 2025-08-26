#ifndef localityAllocator_h
#define localityAllocator_h

template <size_t LocalityBlockDistance, size_t LocalityBinDistance, size_t MinBlockSize>
class LocalityArenaAllocator {
public:
	typedef unsigned int LocalityId;
private:
	struct BlockAllocation {
		LocalityId localityId;
		std::unique_ptr<char[]> allocation; // char is always 1 byte
		size_t size;
		size_t allocationCount;
		size_t usedStartIndex;
		size_t usedEndIndex;
	};
public:
	LocalityArenaAllocator() {
		assert(LocalityBinDistance > 0);
		assert(LocalityBlockDistance > 0);
		assert(MinBlockSize > 0);
	}

	void* Allocate(size_t allocationSize, LocalityId localityId) {
		if (allocationSize == 0) return nullptr;
		size_t binIndex = localityId >> (LocalityBinDistance - 1);
		allocatedBytes += allocationSize;
		allocationCount++;
		if (allocationSize >= MinBlockSize) { // if the allocation is very large we just use one block allocation
			BlockAllocation& blockAllocation = createBlockAllocation(allocationSize, binIndex);
			blockAllocation.localityId = localityId;
			blockAllocation.usedEndIndex = blockAllocation.size;
			blockAllocation.allocationCount = 1;
			return blockAllocation.allocation.get();
		} 
		
		if (blockAllocationBins.size() <= binIndex) {
			blockAllocationBins.resize(binIndex + 1);
		} else {
			for (BlockAllocation& blockAllocation : blockAllocationBins[binIndex]) {
				if (
					int(localityId) - int(blockAllocation.localityId) >= -int(LocalityBlockDistance) &&
					int(localityId) - int(blockAllocation.localityId) <= int(LocalityBlockDistance)
				) {
					if (blockAllocation.usedStartIndex >= allocationSize) {
						blockAllocation.usedStartIndex -= allocationSize;
						blockAllocation.allocationCount++;
						return blockAllocation.allocation.get() + blockAllocation.usedStartIndex;
					}
					if (blockAllocation.size - blockAllocation.usedEndIndex >= allocationSize) {
						blockAllocation.usedEndIndex += allocationSize;
						blockAllocation.allocationCount++;
						return blockAllocation.allocation.get() + blockAllocation.usedEndIndex - allocationSize;
					}
				}
			}
		}
		BlockAllocation& blockAllocation = createBlockAllocation(MinBlockSize, blockAllocationBins[binIndex]);
		blockAllocation.localityId = localityId;
		blockAllocation.usedEndIndex = allocationSize;
		blockAllocation.allocationCount = 1;
		return blockAllocation.allocation.get();
	}

	void Deallocate(void* allocation, size_t allocationSize) {
		if (allocationSize == 0) return;
		std::optional<std::pair<size_t, size_t>> blockAllocationLocation = findBlockAllocationContainingPointer(allocation);
		if (!blockAllocationLocation) return;
		std::vector<BlockAllocation>& blockAllocationBin = blockAllocationBins[blockAllocationLocation->first];
		BlockAllocation& blockAllocation = blockAllocationBin[blockAllocationLocation->second];
		allocatedBytes -= allocationSize;
		allocationCount--;
		if (blockAllocation.allocationCount == 1) {
			blockAllocationCount--;
			if (blockAllocationBin.size() == 1) {
				blockAllocationBins.erase(blockAllocationBins.begin() + blockAllocationLocation->first);
				return;
			}
			blockAllocationBin.erase(blockAllocationBin.begin() + blockAllocationLocation->second);
			return;
		}
		blockAllocation.allocationCount--;
		if (blockAllocation.allocation.get() + blockAllocation.usedStartIndex == allocation) {
			blockAllocation.usedStartIndex += allocationSize;
			return;
		}
		if (blockAllocation.allocation.get() + blockAllocation.usedEndIndex - allocationSize == allocation) {
			blockAllocation.usedEndIndex -= allocationSize;
			return;
		}
		// We lose the allocation otherwise. Do better later?
	}

	void printStats() const {
		logInfo("Allocated bytes: {}, Allocation count: {}, Block allocation count: {}", "LocalityArenaAllocator", allocatedBytes, allocationCount, blockAllocationCount);
		logInfo("Average allocation size: {}, ", "LocalityArenaAllocator", (float)allocatedBytes/(float)allocationCount);
		logInfo("Average allocated bytes per block allocation: {}, ", "LocalityArenaAllocator", (float)allocatedBytes/(float)blockAllocationCount);
	}

	template<class T>
	void printStats() const {
		logInfo("Allocated bytes: {}, Allocation count: {}, Block allocation count: {}", "LocalityArenaAllocator", allocatedBytes/sizeof(T), allocationCount, blockAllocationCount);
		logInfo("Average allocation size: {}, ", "LocalityArenaAllocator", (float)(allocatedBytes/sizeof(T))/(float)allocationCount);
		logInfo("Average allocated bytes per block allocation: {}, ", "LocalityArenaAllocator", (float)(allocatedBytes/sizeof(T))/(float)blockAllocationCount);
	}

private:
	BlockAllocation& createBlockAllocation(size_t allocationSize, std::vector<BlockAllocation>& blockAllocationBin) {
		blockAllocationCount++;
		blockAllocationBin.emplace_back(0, std::make_unique<char[]>(allocationSize), allocationSize, 0, 0, 0);
		return blockAllocationBin.back();
	}

	BlockAllocation& createBlockAllocation(size_t allocationSize, size_t binIndex) {
		blockAllocationCount++;
		if (blockAllocationBins.size() <= binIndex) {
			blockAllocationBins.resize(binIndex + 1);
		}
		blockAllocationBins[binIndex].emplace_back(0, std::make_unique<char[]>(allocationSize), allocationSize, 0, 0, 0);
		return blockAllocationBins[binIndex].back();
	}

	std::optional<std::pair<size_t, size_t>> findBlockAllocationContainingPointer(void* ptr) const {
		for (size_t blockAllocationBinIndex = 0; blockAllocationBinIndex < blockAllocationBins.size(); blockAllocationBinIndex++) {
			for (size_t blockAllocationIndex = 0; blockAllocationIndex < blockAllocationBins[blockAllocationBinIndex].size(); blockAllocationIndex++) {
				const BlockAllocation& blockAllocation = blockAllocationBins[blockAllocationBinIndex][blockAllocationIndex];
				if (blockAllocation.allocation.get() <= ptr && blockAllocation.allocation.get() + blockAllocation.size > ptr) {
					return std::make_pair(blockAllocationBinIndex, blockAllocationIndex);
				}
			}
		}
		return std::nullopt;
	}
	
	unsigned int allocatedBytes = 0;
	unsigned int allocationCount = 0;
	unsigned int blockAllocationCount = 0;
	std::vector<std::vector<BlockAllocation>> blockAllocationBins;
};
 
template<class T, size_t LocalityBlockDistance, size_t LocalityBinDistance, size_t MinBlockSize>
class LocalityAllocator {
public:
    typedef T value_type;

	typedef LocalityArenaAllocator<LocalityBlockDistance, LocalityBinDistance, MinBlockSize>::LocalityId LocalityId;

	LocalityAllocator() { assert(false); }
	
    LocalityAllocator(
		std::shared_ptr<LocalityArenaAllocator<LocalityBlockDistance, LocalityBinDistance, MinBlockSize>> localityArenaAllocator,
		LocalityId localityId
	) : localityArenaAllocator(localityArenaAllocator), localityId(localityId) { assert(localityArenaAllocator.get()); }

	template<class OtherT>
    LocalityAllocator(LocalityAllocator<OtherT, LocalityBlockDistance, LocalityBinDistance, MinBlockSize>& other) :
		localityArenaAllocator(other.localityArenaAllocator), localityId(other.localityId) { assert(localityArenaAllocator.get()); }
 
	template<class OtherT>
    LocalityAllocator(LocalityAllocator<OtherT, LocalityBlockDistance, LocalityBinDistance, MinBlockSize>&& other) :
		localityArenaAllocator(std::move(other.localityArenaAllocator)), localityId(other.localityId) { assert(localityArenaAllocator.get()); }
	
	
	void swap(LocalityAllocator<T, LocalityBlockDistance, LocalityBinDistance, MinBlockSize>& other) {
		std::swap(this->localityId, other.localityId);
		std::swap(this->localityArenaAllocator, other.localityArenaAllocator);
	}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::bad_array_new_length();
 
		return (T*)localityArenaAllocator->Allocate(n * sizeof(T), localityId);
    }
 
    void deallocate(T* p, std::size_t n) noexcept {
		return localityArenaAllocator->Deallocate(p, n * sizeof(T));
    }

	template<class OtherT, size_t OtherLocalityBlockDistance, size_t OtherLocalityBinDistance, size_t OtherMinBlockSize>
	bool operator==(const LocalityAllocator<OtherT, OtherLocalityBlockDistance, OtherLocalityBinDistance, OtherMinBlockSize>& other) {
		return localityArenaAllocator.get() == other.localityArenaAllocator.get();
	}

	template<class OtherT, size_t OtherLocalityBlockDistance, size_t OtherLocalityBinDistance, size_t OtherMinBlockSize>
	bool operator!=(const LocalityAllocator<OtherT, OtherLocalityBlockDistance, OtherLocalityBinDistance, OtherMinBlockSize>& other) { return !(*this == other); }

	template<class OtherT>
    struct rebind {
        using other = LocalityAllocator<OtherT, LocalityBlockDistance, LocalityBinDistance, MinBlockSize>;
    };

private:
	std::shared_ptr<LocalityArenaAllocator<LocalityBlockDistance, LocalityBinDistance, MinBlockSize>> localityArenaAllocator;
	LocalityArenaAllocator<LocalityBlockDistance, LocalityBinDistance, MinBlockSize>::LocalityId localityId;
};

template<class T, size_t LocalityBlockDistance, size_t LocalityBinDistance, size_t MinBlockSize>
class LocalityVectorGenerator {
public:
	LocalityVectorGenerator() : localityArenaAllocator(std::make_shared<LocalityArenaAllocator<LocalityBlockDistance, LocalityBinDistance, MinBlockSize>>()) {}

	typedef LocalityArenaAllocator<LocalityBlockDistance, LocalityBinDistance, MinBlockSize>::LocalityId LocalityId;
	typedef std::vector<T, LocalityAllocator<T, LocalityBlockDistance, LocalityBinDistance, MinBlockSize>> LocalityVector;

    LocalityVector getVector(LocalityId localityId) {
		return LocalityVector(LocalityAllocator<T, LocalityBlockDistance, LocalityBinDistance, MinBlockSize>(localityArenaAllocator, localityId));
	}

	const LocalityArenaAllocator<LocalityBlockDistance, LocalityBinDistance, MinBlockSize>& getAllocator() const {
		return *localityArenaAllocator;
	}

private:
	std::shared_ptr<LocalityArenaAllocator<LocalityBlockDistance, LocalityBinDistance, MinBlockSize>> localityArenaAllocator;
};

#endif /* localityAllocator_h */
