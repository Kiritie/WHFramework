#pragma once

#include "CoreMinimal.h"
#include "Voxel/WorldObject/VoxelWorldObjectRegistry.h"
#include "Voxel/Save/VoxelDeltaTypes.h"

class UVoxelModule;
class AVoxelWorldObjectPresenter;

class WHFRAMEWORK_API FVoxelWorldObjectPresenterView
{
public:
	FVoxelWorldObjectPresenterView();
	~FVoxelWorldObjectPresenterView();
	void Reset();
	void Invalidate(UVoxelModule& InModule, const FVoxelWorldObjectRegistry& InObjects, const FVoxelEditBatch& InBatch, TFunctionRef<bool(const FVoxelWorldObjectInstance&)> InIsActive);
	void Refresh(UVoxelModule& InModule, const FVoxelWorldObjectRegistry& InObjects, const TArray<FVector>& InObservers, TFunctionRef<bool(const FVoxelWorldObjectInstance&)> InIsActive);
	AVoxelWorldObjectPresenter* Find(const FIntVector& InAnchor) const;

private:
	struct FSectionObjects
	{
		uint64 Token = 0;
		uint64 Revision = MAX_uint64;
		TArray<FIntVector> Anchors;
	};

	TMap<FIntVector, FSectionObjects> Sections;
	TMap<FIntVector, TWeakObjectPtr<AVoxelWorldObjectPresenter>> Actors;
	TSet<FIntVector> DirtySections;
};
