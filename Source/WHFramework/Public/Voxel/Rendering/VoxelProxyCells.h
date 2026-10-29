#pragma once

#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelBlockState.h"

struct WHFRAMEWORK_API FVoxelProxyColumn
{
	int32 Ground = -1;
	int32 Water = -1;
	FVoxelBlockState Surface;
	FVoxelBlockState WaterState;
};

struct WHFRAMEWORK_API FVoxelProxyCell
{
	int32 Index = 0;
	FVoxelBlockState State;
};

// 自然地表按列描述，地下实体仅按需隐式查询；结构与修改保存稀疏状态。
class WHFRAMEWORK_API FVoxelProxyCells
{
public:
	class FReference
	{
	public:
		FReference(FVoxelProxyCells& InOwner, int32 InIndex) : Owner(InOwner), Index(InIndex) {}
		operator FVoxelBlockState() const { return Owner.Get(Index); }
		FReference& operator=(FVoxelBlockState InState) { Owner.Set(Index, InState); return *this; }
		FReference& operator=(const FReference& InOther) { return *this = FVoxelBlockState(InOther); }
		uint32 Pack() const { return Owner.Get(Index).Pack(); }
		bool IsAir() const { return Owner.Get(Index).IsAir(); }
	private:
		FVoxelProxyCells& Owner;
		int32 Index;
	};

	void Init(FVoxelBlockState InState, int32 InCount);
	void SetNum(int32 InCount) { Init(FVoxelBlockState(), InCount); }
	void Add(FVoxelBlockState InState);
	void InitializeColumns(int32 InSide, FVoxelBlockState InInterior);
	void SetColumn(int32 InIndex, const FVoxelProxyColumn& InColumn);
	void AppendShard(const FVoxelProxyCells& InShard, FIntVector InOffset);
	void Sort();
	FVoxelBlockState Get(int32 InIndex) const;
	void Set(int32 InIndex, FVoxelBlockState InState);
	FVoxelBlockState operator[](int32 InIndex) const { return Get(InIndex); }
	FReference operator[](int32 InIndex) { return FReference(*this, InIndex); }
	int32 Num() const { return Count; }
	bool IsEmpty() const { return Count == 0; }
	bool IsValidIndex(int32 InIndex) const { return InIndex >= 0 && InIndex < Count; }
	uint64 GetAllocatedSize() const;
	bool Validate(int32 InSide) const;
	bool ContainsByPredicate(TFunctionRef<bool(const FVoxelBlockState&)> InPredicate) const;
	void Transform(TFunctionRef<void(FVoxelBlockState&)> InTransform);
	bool MayHaveSurface(FIntVector InOffset, int32 InSide) const;
	FVoxelBlockState GetInterior() const { return Interior; }
	const TArray<FVoxelProxyColumn>& GetColumns() const { return Columns; }
	const TArray<FVoxelProxyCell>& GetOverrides() const { return Overrides; }
	bool Import(int32 InSide, FVoxelBlockState InInterior, TArray<FVoxelProxyColumn>&& InColumns, TArray<FVoxelProxyCell>&& InOverrides);

private:
	FVoxelBlockState GetNatural(int32 InIndex) const;
	int32 LowerBound(int32 InIndex) const;
	void ReserveOverrides(int32 InCount);
	int32 Count = 0;
	int32 Side = 0;
	FVoxelBlockState Interior;
	TArray<FVoxelProxyColumn> Columns;
	TArray<FVoxelProxyCell> Overrides;
	bool bSorted = true;
};
