#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

// 稳定索引的原生分页正文；增长只分配新页，所有 backing 仍由原生成账本授予。
template<typename ElementType, int32 ElementsPerPage = 1024>
class TVoxelGenerationPagedArray
{
    static_assert(ElementsPerPage > 0);

public:
    TVoxelGenerationPagedArray() = default;
    TVoxelGenerationPagedArray(const TVoxelGenerationPagedArray&) = delete;
    TVoxelGenerationPagedArray& operator=(const TVoxelGenerationPagedArray&) = delete;

    int32 Num() const { return Count; }
    bool IsEmpty() const { return Count == 0; }

    bool ReserveOne(FString& OutError)
    {
        if (Count == MAX_int32)
        {
            OutError = TEXT("Oversized: native paged array element count");
            return false;
        }
        const int32 PageIndex = Count / ElementsPerPage;
        if (PageIndex < Pages.Num()) return true;
        if (Pages.Num() == Pages.Max())
        {
            const int32 Capacity = int32(FMath::Min<int64>(MAX_int32,
                FMath::Max<int64>(4, int64(Pages.Max()) * 2)));
            if (!VoxelGenerationAllocation::RetainArray(Pages, Capacity, OutError)) return false;
        }
        TArray<ElementType> Page;
        if (!VoxelGenerationAllocation::RetainArray(Page, ElementsPerPage, OutError)) return false;
        PageBytes += VoxelGenerationAllocation::AllocatedBytes(Page);
        Pages.Add(MoveTemp(Page));
        return true;
    }

    int32 Add(const ElementType& InElement)
    {
        check(Pages.IsValidIndex(Count / ElementsPerPage));
        TArray<ElementType>& Page = Pages[Count / ElementsPerPage];
        check(Page.Num() == Count % ElementsPerPage && Page.Num() < Page.Max());
        Page.Add(InElement);
        return Count++;
    }

    ElementType& operator[](int32 InIndex)
    {
        check(InIndex >= 0 && InIndex < Count);
        return Pages[InIndex / ElementsPerPage][InIndex % ElementsPerPage];
    }

    const ElementType& operator[](int32 InIndex) const
    {
        check(InIndex >= 0 && InIndex < Count);
        return Pages[InIndex / ElementsPerPage][InIndex % ElementsPerPage];
    }

    uint64 GetAllocatedBytes() const
    {
        return PageBytes + VoxelGenerationAllocation::AllocatedBytes(Pages);
    }

    void FreeBacking()
    {
        for (TArray<ElementType>& Page : Pages)
        {
            VoxelGenerationAllocation::FreeRetainedArray(Page);
        }
        VoxelGenerationAllocation::FreeRetainedArray(Pages);
        PageBytes = 0;
        Count = 0;
    }

private:
    TArray<TArray<ElementType>> Pages;
    uint64 PageBytes = 0;
    int32 Count = 0;
};
