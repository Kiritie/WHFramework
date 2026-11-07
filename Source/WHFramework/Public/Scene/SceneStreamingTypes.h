#pragma once

#include "CoreMinimal.h"
#include "Parameter/ParameterModuleTypes.h"

#include "SceneStreamingTypes.generated.h"

class APlayerController;

UENUM(BlueprintType)
enum class ESceneLevelLoadResult : uint8
{
    Succeeded,
    Failed,
    Cancelled
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnAsyncLoadLevelFinished, FName, InLevelPath, ESceneLevelLoadResult, Result);

UENUM(BlueprintType)
enum class EFAsyncLoadLevelState : uint8
{
	None,
	//加载
	Loading,
	//卸载
	Unloading
};


USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSoftLevelPath
{
	GENERATED_BODY()

public:
	FSoftLevelPath()
	{
		LevelObjectPtr = nullptr;
		LevelPath = NAME_None;
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UWorld> LevelObjectPtr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides, EditCondition = "!LevelObjectPtr.IsValid()"))
	FName LevelPath;
};

USTRUCT()
struct FSoftLevelPathArrayParameterValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FSoftLevelPath> Value;
};

/**
 *
 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAsyncLoadLevelTask
{
	GENERATED_BODY()

public:
	FAsyncLoadLevelTask()
	{
		bLoading = false;
		State = EFAsyncLoadLevelState::None;
		LevelPath = NAME_None;
		OnLoadFinished = FOnAsyncLoadLevelFinished();
		FinishDelayTime = 0.f;
		bCreateLoadingWidget = false;
	}

public:
    int32 OperationId = 0;
    TWeakObjectPtr<UWorld> OperationWorld;
    TArray<TWeakObjectPtr<APlayerController>> LoadingWidgetPlayers;

	UPROPERTY(VisibleAnywhere)
	bool bLoading;

	UPROPERTY(VisibleAnywhere)
	EFAsyncLoadLevelState State;

	UPROPERTY(VisibleAnywhere)
	FName LevelPath;

	UPROPERTY(VisibleAnywhere)
	FOnAsyncLoadLevelFinished OnLoadFinished;

	UPROPERTY(VisibleAnywhere)
	float FinishDelayTime;

	UPROPERTY(VisibleAnywhere)
	bool bCreateLoadingWidget;
};

/**
 *
 */
