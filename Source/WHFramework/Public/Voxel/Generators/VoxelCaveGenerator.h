#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Math/MathTypes.h"
#include "VoxelCaveGenerator.generated.h"

struct FVoxelCaveSegment
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	float Radius = 1.5f;
};

struct FVoxelCaveSystem
{
	TArray<FVoxelCaveSegment> Segments;
};

/** 体素矿洞生成器 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelCaveGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelCaveGenerator();

	virtual void Generate(UVoxelChunk* InChunk) override;

protected:
	bool TryCreateCaveSystem(FIndex InAnchorChunkIndex, FVoxelCaveSystem& OutSystem) const;

	void GetCaveSystems(FIndex InMinWorldIndex, FIndex InMaxWorldIndex, TArray<FVoxelCaveSystem>& OutSystems) const;

	void CarveSystemSlice(UVoxelChunk* InChunk, const FVoxelCaveSystem& InSystem) const;

	void AddBranch(FRandomStream& InStream, const FVector& InStart, float InYaw, float InPitch, FVoxelCaveSystem& OutSystem) const;

	float PointSegmentDistanceSquared(const FVector& InPoint, const FVoxelCaveSegment& InSegment) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 SystemSpacingChunks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SystemSpawnRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 MinSegments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 MaxSegments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0"))
	float MinSegmentLength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0"))
	float MaxSegmentLength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.5"))
	float MinTunnelRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.5"))
	float MaxTunnelRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EntranceRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BranchRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RoomRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0"))
	float MaxRoomRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 MinSurfaceDepth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 MinWorldHeight;
};
