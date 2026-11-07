#pragma once

#include "CoreMinimal.h"
#include "Scene/SceneMarkerTypes.h"
#include "Task/TaskExecutionTypes.h"
#include "Task/TaskReferenceTypes.h"
#include "Task/TaskObjectiveTypes.h"

#include "TaskPresentationTypes.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FTaskMarkerData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bVisible = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor Color = FLinearColor(1.f, 0.75f, 0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Priority = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Bitmask, BitmaskEnum = "/Script/WHFramework.ESceneMarkerChannel"))
	int32 Channels = static_cast<int32>(ESceneMarkerChannel::Map | ESceneMarkerChannel::MiniMap |
		ESceneMarkerChannel::Compass | ESceneMarkerChannel::World);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bOnlyShowNavigationWhenTracked = true;
};

USTRUCT()
struct WHFRAMEWORK_API FTaskPresentationState
{
	GENERATED_BODY()

	UPROPERTY()
	FString TaskGUID;

	UPROPERTY()
	ETaskState State = ETaskState::None;

	UPROPERTY()
	ETaskExecuteResult Result = ETaskExecuteResult::None;

	UPROPERTY()
	FTaskTarget Target;

	UPROPERTY()
	TMap<FName, int32> Progress;

	UPROPERTY()
	TMap<FName, int32> RequiredCounts;
};

USTRUCT()
struct WHFRAMEWORK_API FTaskAssetPresentation
{
	GENERATED_BODY()

	UPROPERTY()
	FTaskAssetSaveData Asset;

	UPROPERTY()
	TArray<FTaskPresentationState> Tasks;
};
