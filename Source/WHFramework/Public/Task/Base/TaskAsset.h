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

	//////////////////////////////////////////////////////////////////////////
	/// Task Stats
public:
	/// 自动进入初始任务
	UPROPERTY(EditAnywhere)
	bool bAutoEnterFirst;
	/// 初始任务
	UPROPERTY(VisibleAnywhere)
	UTaskBase* FirstTask;
	/// 根任务
	UPROPERTY(VisibleAnywhere)
	TArray<UTaskBase*> RootTasks;
	/// 任务Map
	UPROPERTY(Transient)
	TMap<FString, UTaskBase*> TaskMap;

	//////////////////////////////////////////////////////////////////////////
	/// Editor
public:
#if WITH_EDITOR
	void GenerateTaskListItem(TArray<TSharedPtr<struct FTaskListItem>>& OutTaskListItems, const FString& InFilterText = TEXT(""));

	void UpdateTaskListItem(TArray<TSharedPtr<struct FTaskListItem>>& OutTaskListItems);

	bool CanAddTask(TSubclassOf<UTaskBase> InTaskClass);

	void ClearAllTask();
#endif
};
