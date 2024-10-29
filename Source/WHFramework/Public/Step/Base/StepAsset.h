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
	GENERATED_BODY()

public:
	UStepAsset(const FObjectInitializer& ObjectInitializer);

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
	UPROPERTY(Transient)
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
	void GenerateStepListItem(TArray<TSharedPtr<struct FStepListItem>>& OutStepListItems, const FString& InFilterText = TEXT(""));

	void UpdateStepListItem(TArray<TSharedPtr<struct FStepListItem>>& OutStepListItems);

	bool CanAddStep(TSubclassOf<UStepBase> InStepClass);

	void ClearAllStep();
#endif
};
