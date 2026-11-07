#pragma once

#include "CoreMinimal.h"
#include "Parameter/ParameterModuleTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Task/TaskReferenceTypes.h"
#include "Task/TaskObjectiveTypes.h"

#include "TaskSaveTypes.generated.h"

USTRUCT()
struct WHFRAMEWORK_API FTaskRuntimeSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	FParameter Data;

	UPROPERTY()
	float ExecuteRemaining = -1.f;

	UPROPERTY()
	float CompleteRemaining = -1.f;

	UPROPERTY()
	float LeaveRemaining = -1.f;

	UPROPERTY()
	float GuideRemaining = -1.f;

	UPROPERTY()
	FTaskTarget Target;

	UPROPERTY()
	TMap<FName, int32> ObjectiveRequiredCounts;

	UPROPERTY()
	TMap<FName, FName> ObjectiveTargetNames;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FTaskModuleSaveData : public FSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FTaskAssetSaveData> Assets;

	UPROPERTY()
	TMap<FString, FTaskRuntimeSaveData> TaskRecords;

};
