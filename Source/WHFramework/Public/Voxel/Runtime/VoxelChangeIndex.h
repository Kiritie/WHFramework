#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

struct FVoxelRegionDirectorySnapshot;

class WHFRAMEWORK_API FVoxelChangeStorage
{
public:
	FVoxelChangeStorage() = default;
	FVoxelChangeStorage(FVoxelChangeStorage&&) = default;
	FVoxelChangeStorage& operator=(FVoxelChangeStorage&&) = default;
	FVoxelChangeStorage(const FVoxelChangeStorage&) = delete;
	FVoxelChangeStorage& operator=(const FVoxelChangeStorage&) = delete;
	void BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	bool Fail(EVoxelTerrainResourceAdmission InAdmission, FString& OutError);
	EVoxelTerrainResourceAdmission GetAdmission() const { return Admission; }
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> GetResources() const { return Resources; }
	FVoxelGenerationConsumerKey GetConsumer() const { return Consumer; }
	uint64 GetBytes() const { return Bytes; }
	int32 GetLeaseCount() const { return int32(Lease.IsValid()); }
	const FVoxelTerrainResourceLease* GetLease() const { return Lease.Get(); }
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ShareLease() const { return Lease; }
	void SetRequirements(uint64 InInputBytes, int32 InLeaseCount)
	{
		RequiredInputBytes = InInputBytes;
		RequiredLeaseCount = InLeaseCount;
	}
	void Succeed()
	{
		Admission = EVoxelTerrainResourceAdmission::Granted;
	}
	void Release();
	template<typename ElementType>
	bool Reserve(TSet<ElementType>& InSet, const int32 InCount, FString& OutError)
	{
		if (InCount < 0) return Fail(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
		if (VoxelGenerationAllocation::Private::SetCanReserve<ElementType>(InSet, InCount))
		{
			Succeed();
			return true;
		}
		const uint64 OldBytes = VoxelGenerationAllocation::AllocatedBytes(InSet);
		const uint64 NewBytes = VoxelGenerationAllocation::Private::SetReserveBytes<ElementType>(InSet, InCount);
		if (!Prepare(NewBytes, OutError)) return false;
		InSet.Reserve(InCount);
		const uint64 Actual = VoxelGenerationAllocation::AllocatedBytes(InSet);
		check(Actual <= NewBytes && OldBytes <= Bytes - NewBytes);
		Bytes = Bytes - OldBytes - NewBytes + Actual;
		if (Lease)
		{
			const bool bSettled = Lease->Resize(Bytes);
			check(bSettled);
		}
		return true;
	}

	template<typename KeyType, typename ValueType>
	bool Reserve(TMap<KeyType, ValueType>& InMap, const int32 InCount, FString& OutError)
	{
		if (InCount < 0) return Fail(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
		if (VoxelGenerationAllocation::Private::SetCanReserve<TPair<KeyType, ValueType>>(InMap, InCount))
		{
			Succeed();
			return true;
		}
		const uint64 OldBytes = VoxelGenerationAllocation::AllocatedBytes(InMap);
		const uint64 NewBytes = VoxelGenerationAllocation::Private::SetReserveBytes<TPair<KeyType, ValueType>>(InMap, InCount);
		if (!Prepare(NewBytes, OutError)) return false;
		InMap.Reserve(InCount);
		const uint64 Actual = VoxelGenerationAllocation::AllocatedBytes(InMap);
		check(Actual <= NewBytes && OldBytes <= Bytes - NewBytes);
		Bytes = Bytes - OldBytes - NewBytes + Actual;
		if (Lease)
		{
			const bool bSettled = Lease->Resize(Bytes);
			check(bSettled);
		}
		return true;
	}

private:
	bool Prepare(uint64 InNewBytes, FString& OutError);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	uint64 Bytes = 0;
	uint64 RequiredInputBytes = 0;
	int32 RequiredLeaseCount = 0;
	EVoxelTerrainResourceAdmission Admission = EVoxelTerrainResourceAdmission::None;
};

class WHFRAMEWORK_API FVoxelChangeIndex
{
public:
	void SetBaseline(TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe> InBaseline);
	void BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	bool PrepareModified(int32 InCount, TFunctionRef<FIntVector(int32)> InSection, FString& OutError);
	bool SetModified(const FIntVector& InSection, bool bInModified, FString& OutError);
	EVoxelTerrainResourceAdmission GetLastResourceAdmission() const { return Storage.GetAdmission(); }
	FVoxelGenerationConsumerKey GetControlConsumer() const { return Storage.GetConsumer(); }
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> GetStorageLease() const;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> GetBaselineLease() const;
	bool IsModified(const FIntVector& InSection) const;
	bool SetRegionMask(const FIntVector& InRegion, TConstArrayView<uint64> InMask, FString& OutError);
	void GetRegionMask(const FIntVector& InRegion, TArrayView<uint64> OutMask) const;
	bool Enumerate(const FVoxelGenerationBounds& InBounds, TArray<FIntVector>& OutSections,
		FVoxelGenerationWorkspace& InWorkspace, FString& OutError, const TAtomic<bool>* InCancel = nullptr) const;
#if WITH_DEV_AUTOMATION_TESTS
	void SetModified(const FIntVector& InSection, bool bInModified);
	void SetRegionMask(const FIntVector& InRegion, const TArray<uint64>& InMask);
	void GetRegionMask(const FIntVector& InRegion, TArray<uint64>& OutMask) const;
	bool Enumerate(
		const FVoxelGenerationBounds& InBounds,
		TArray<FIntVector>& OutSections,
		const TAtomic<bool>* InCancel = nullptr) const;
#endif
	bool HasAnyModified() const;
	int32 Count(const FVoxelGenerationBounds& InBounds) const;
	bool Visit(const FVoxelGenerationBounds& InBounds, TFunctionRef<void(const FIntVector&)> InVisit,
		const TAtomic<bool>* InCancel = nullptr) const;
	void Reset();

private:
	static int32 PositiveMod(int32 InValue, int32 InDivisor);
	static FIntVector ToRegion(const FIntVector& InSection);
	static int32 ToRegionBit(const FIntVector& InSection);
	static FIntVector RegionBitToSection(const FIntVector& InRegion, int32 InBit);
	bool VisitLocked(const FVoxelGenerationBounds& InBounds, TFunctionRef<void(const FIntVector&)> InVisit,
		const TAtomic<bool>* InCancel) const;

private:
	mutable FRWLock Lock;
	FVoxelChangeStorage Storage;
	struct FRegionMask
	{
		uint64 Words[8] = {};
	};
	TMap<FIntVector, FRegionMask> RegionMasks;
	TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe> Baseline;
	uint64 ModifiedCount = 0;
};
