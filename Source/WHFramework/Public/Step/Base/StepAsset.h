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
	/// 自动开始初始步骤
	UPROPERTY(EditAnywhere)
	bool bAutoStartFirst;
	/// 初始步骤 
	UPROPERTY(VisibleAnywhere)
	UStepBase* FirstStep;
	/// 根步骤
	UPROPERTY(VisibleAnywhere)
	TArray<UStepBase*> RootSteps;
	/// 步骤Map
	UPROPERTY(Transient)
	TMap<FString, UStepBase*> StepMap;

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
