// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Asset/Base/AssetBase.h"
#include "TaskAsset.generated.h"

class UTaskBase;
/**
 * Imported spreadsheet table.
 */
UCLASS(BlueprintType, Meta = (LoadBehavior = "LazyOnDemand"))
class WHFRAMEWORK_API UTaskAsset : public UAssetBase
{
	GENERATED_BODY()

public:
	UTaskAsset(const FObjectInitializer& ObjectInitializer);

public:
	virtual void Initialize() override;
	virtual UWorld* GetWorld() const override;
	bool RebuildTaskMap(bool bValidateDefinition = true);
	bool ValidateTasks(TArray<FText>& OutErrors) const;

	//////////////////////////////////////////////////////////////////////////
	/// Task Stats
public:
	/// 根任务
	UPROPERTY(VisibleAnywhere, Instanced)
	TArray<UTaskBase*> RootTasks;
	/// 任务Map
	UPROPERTY(Transient)
	TMap<FString, UTaskBase*> TaskMap;

public:
	bool IsAllTaskCompleted() const;
	
	bool IsAllTaskLeaved() const;

	//////////////////////////////////////////////////////////////////////////
	/// Editor
public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;

	bool CanAddTask(TSubclassOf<UTaskBase> InTaskClass);

	void ClearAllTask();
#endif
};
