#pragma once

#include "CoreMinimal.h"
#include "Misc/Crc.h"
#include "ProceduralMeshComponent.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

static_assert(sizeof(FVector) == 3 * sizeof(double));
static_assert(sizeof(FVector2D) == 2 * sizeof(double));
static_assert(sizeof(FLinearColor) == 4 * sizeof(float));

template<typename ValueType>
struct TVoxelMeshAttributeTraits
{
	static uint32 Hash(const ValueType& InValue)
	{
		return FCrc::MemCrc32(&InValue, sizeof(ValueType));
	}
	static bool Equal(const ValueType& InFirst, const ValueType& InSecond)
	{
		return FMemory::Memcmp(&InFirst, &InSecond, sizeof(ValueType)) == 0;
	}
};

template<>
struct TVoxelMeshAttributeTraits<FProcMeshTangent>
{
	static uint32 Hash(const FProcMeshTangent& InValue)
	{
		return HashCombineFast(FCrc::MemCrc32(&InValue.TangentX, sizeof(InValue.TangentX)), uint32(InValue.bFlipTangentY));
	}
	static bool Equal(const FProcMeshTangent& InFirst, const FProcMeshTangent& InSecond)
	{
		return FMemory::Memcmp(&InFirst.TangentX, &InSecond.TangentX, sizeof(InFirst.TangentX)) == 0 &&
			InFirst.bFlipTangentY == InSecond.bFlipTangentY;
	}
};

// 未发布时写入展开流，冻结前可将精确值改存为有界字典；读取不创建解码缓存。
template<typename ValueType>
class TVoxelMeshAttributeStream
{
public:
	int32 Num() const { return IsPacked() ? Indices.Num() : Values.Num(); }
	int32 Max() const { return IsPacked() ? Indices.Max() : Values.Max(); }
	bool IsEmpty() const { return Num() == 0; }
	bool IsPacked() const { return !Indices.IsEmpty(); }
	bool IsValidIndex(const int32 InIndex) const { return InIndex >= 0 && InIndex < Num(); }
	uint64 GetAllocatedSize() const
	{
		constexpr uint32 Alignment = TAllocatorTraits<FDefaultAllocator>::SupportsElementAlignment ? alignof(ValueType) : DEFAULT_ALIGNMENT;
		return VoxelBoundedAllocation::QuantizeBytes(Values.GetAllocatedSize(), Alignment) +
			VoxelBoundedAllocation::QuantizeBytes(Indices.GetAllocatedSize(),
				TAllocatorTraits<FDefaultAllocator>::SupportsElementAlignment ? alignof(uint8) : DEFAULT_ALIGNMENT);
	}
	uint64 Bytes() const { return uint64(Values.Num()) * sizeof(ValueType) + uint64(Indices.Num()); }
	const ValueType& operator[](const int32 InIndex) const
	{
		return IsPacked() ? Values[Indices[InIndex]] : Values[InIndex];
	}
	void Reserve(const int32 InCount)
	{
		check(!IsPacked());
		Values.Reserve(InCount);
	}
	bool ReserveBounded(const int32 InCount, const uint64 InBytes)
	{
		check(!IsPacked());
		return VoxelBoundedAllocation::Reserve(Values, InCount, InBytes);
	}
	int32 Add(const ValueType& InValue)
	{
		check(!IsPacked());
		return Values.Add(InValue);
	}
	void Init(const ValueType& InValue, const int32 InCount)
	{
		Indices.Empty();
		Values.Init(InValue, InCount);
	}
	void Reset(const int32 InCount = 0)
	{
		Indices.Empty();
		Values.Reset(InCount);
	}
	void Empty(const int32 InCount = 0)
	{
		Indices.Empty();
		Values.Empty(InCount);
	}
	void Append(const TVoxelMeshAttributeStream& InSource)
	{
		check(this != &InSource && !IsPacked());
		Values.Reserve(Values.Num() + InSource.Num());
		for (int32 Index = 0; Index < InSource.Num(); ++Index) Values.Add(InSource[Index]);
	}
	struct FConstIterator
	{
		const TVoxelMeshAttributeStream* Stream;
		int32 Index;
		const ValueType& operator*() const { return (*Stream)[Index]; }
		FConstIterator& operator++() { ++Index; return *this; }
		bool operator!=(const FConstIterator& InOther) const { return Index != InOther.Index; }
	};
	FConstIterator begin() const { return {this, 0}; }
	FConstIterator end() const { return {this, Num()}; }

	bool Compact(const uint64 InWorkingBytes, const TAtomic<bool>* InCancel = nullptr)
	{
		if (InCancel && InCancel->Load()) return false;
		if (IsPacked() || Values.Num() < 64) return true;
		TArray<ValueType, TFixedAllocator<256>> Dictionary;
		int32 Slots[512];
		for (int32& Slot : Slots) Slot = INDEX_NONE;
		const auto FindSlot = [&](const ValueType& Value)
		{
			uint32 Slot = TVoxelMeshAttributeTraits<ValueType>::Hash(Value) & 511u;
			while (Slots[Slot] != INDEX_NONE && !TVoxelMeshAttributeTraits<ValueType>::Equal(Dictionary[Slots[Slot]], Value))
				Slot = (Slot + 1) & 511u;
			return Slot;
		};
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			if ((Index & 255) == 0 && InCancel && InCancel->Load()) return false;
			const uint32 Slot = FindSlot(Values[Index]);
			if (Slots[Slot] != INDEX_NONE) continue;
			if (Dictionary.Num() == 256) return true;
			Slots[Slot] = Dictionary.Add(Values[Index]);
		}
		const uint64 Required = VoxelBoundedAllocation::ArrayBytes<ValueType>(Dictionary.Num()) +
			VoxelBoundedAllocation::ArrayBytes<uint8>(Values.Num());
		if (Required > InWorkingBytes || Required >= GetAllocatedSize()) return true;
		TArray<ValueType> NewValues;
		TArray<uint8> NewIndices;
		NewValues.Reserve(Dictionary.Num());
		for (const ValueType& Value : Dictionary) NewValues.Add(Value);
		NewIndices.Reserve(Values.Num());
		NewIndices.SetNumUninitialized(Values.Num());
		check(NewValues.GetAllocatedSize() + NewIndices.GetAllocatedSize() <= Required);
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			if ((Index & 255) == 0 && InCancel && InCancel->Load()) return false;
			NewIndices[Index] = uint8(Slots[FindSlot(Values[Index])]);
		}
		if (InCancel && InCancel->Load()) return false;
		Values = MoveTemp(NewValues);
		Indices = MoveTemp(NewIndices);
		return true;
	}

private:
	TArray<ValueType> Values;
	TArray<uint8> Indices;
};

namespace VoxelBoundedAllocation
{
	template<typename ValueType>
	bool Reserve(TVoxelMeshAttributeStream<ValueType>& InStream, const int32 InCount, const uint64 InBytes)
	{
		return InStream.ReserveBounded(InCount, InBytes);
	}
}
