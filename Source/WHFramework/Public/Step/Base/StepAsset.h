// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Asset/Base/AssetBase.h"
#include "StepAsset.generated.h"

class UStepBase;
/**
 * Imported spreadsheet table.
 */
UCLASS(BlueprintType, Meta = (LoadBehavior = "LazyOnDemand"))
class WHFRAMEWORK_API UStepAsset : public UAssetBase
{
	GENERATED_UCLASS_BODY()

public:
	virtual void Initialize(UAssetBase* InSource) override;

	//////////////////////////////////////////////////////////////////////////
	/// Step Stats
public:
	/// 初始步骤 
	UPROPERTY(VisibleAnywhere)
	UStepBase* FirstStep;
	/// 根步骤
	UPROPERTY(VisibleAnywhere)
	TArray<UStepBase*> RootSteps;
	/// 步骤Map
	UPROPERTY(VisibleAnywhere)
	TMap<FString, UStepBase*> StepMap;

public:
	/**
	* 获取初始步骤
	*/
	UFUNCTION(BlueprintPure)
	UStepBase* GetFirstStep() const { return FirstStep; }
	/**
	* 设置初始步骤
	*/
	UFUNCTION(BlueprintCallable)
	void SetFirstStep(UStepBase* InFirstStep) { this->FirstStep = InFirstStep; }
	/**
	* 获取根步骤列表
	*/
	UFUNCTION(BlueprintPure)
	TArray<UStepBase*>& GetRootSteps() { return RootSteps; }
	/**
	* 获取步骤Map
	*/
	UFUNCTION(BlueprintPure)
	TMap<FString, UStepBase*>& GetStepMap() { return StepMap; }

	//////////////////////////////////////////////////////////////////////////
	/// Editor
public:
#if WITH_EDITOR
	void GenerateStepListItem(TArray<TSharedPtr<struct FStepListItem>>& OutStepListItems);

	void UpdateStepListItem(TArray<TSharedPtr<struct FStepListItem>>& OutStepListItems);

	void ClearAllStep();
#endif
};
