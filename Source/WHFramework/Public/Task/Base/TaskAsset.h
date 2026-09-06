// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Asset/Base/AssetBase.h"
#include "Task/TaskModuleTypes.h"
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
	/** 当前运行时副本的唯一标识，无效值表示模板或默认运行时副本 */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly)
	FGuid InstanceID;

	/** 拥有或发布当前运行时副本的角色稳定标识 */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly)
	FGuid AgentID;

	/** 发布者的最后已知位置，在流送角色不存在时保留 */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly)
	FVector AgentLocation = FVector::ZeroVector;

	/// 根任务
	UPROPERTY(VisibleAnywhere, Instanced)
	TArray<UTaskBase*> RootTasks;
	/// 任务Map
	UPROPERTY(Transient)
	TMap<FString, UTaskBase*> TaskMap;

public:
	bool IsAllTaskCompleted() const;
	
	bool IsAllTaskLeaved() const;

	UFUNCTION(BlueprintPure)
	FTaskReference MakeTaskReference(const FString& InTaskGUID) const;

	//////////////////////////////////////////////////////////////////////////
	/// Editor
public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;

	bool CanAddTask(TSubclassOf<UTaskBase> InTaskClass);

	void ClearAllTask();
#endif
};
