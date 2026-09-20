#pragma once

#include "CoreMinimal.h"
#include "VoxelWorldReadiness.generated.h"

UENUM()
enum class EVoxelWorldReadyStage : uint8
{
	None = 0,
	AssetsValidated,
	RecipeFrozen,
	SpawnPlanReady,
	SpawnDataReady,
	SpawnCollisionReady,
	PrimaryViewReady,
	Playable,
	Failed
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelWorldReadinessSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int64 WorldEpoch = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bAssetsValidated = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bRecipeFrozen = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bSpawnPlanReady = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 RequiredSpawnSections = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 ReadySpawnSections = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 RequiredCollisionSections = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 ReadyCollisionSections = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 RequiredPrimaryRepresentations = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 ReadyPrimaryRepresentations = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 RequiredPrimaryFineSections = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 ReadyPrimaryFineSections = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 RenderablePrimaryFineSections = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 PendingCriticalDependencies = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bFailed = false;
};
