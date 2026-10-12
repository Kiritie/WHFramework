#pragma once

#include "CoreMinimal.h"
#include "Common/Base/WHObject.h"
#include "Step/StepModuleTypes.h"
#include "StepRuntime.generated.h"

class UStepBase;
class UStepAsset;
class UStepModule;

UCLASS(Transient)
class WHFRAMEWORK_API UStepRuntime : public UWHObject
{
	GENERATED_BODY()

public:
	UStepRuntime();

	bool Build(UStepAsset* InAsset, TArray<FText>& OutErrors);
	void Shutdown();
	UStepBase* FindStep(const FString& InGUID) const;
	UStepModule* GetModule() const;
	bool Contains(const UStepBase* InStep) const;
	FStepAssetSaveData CaptureData();
	bool RestoreData(const FStepAssetSaveData& InData, FText& OutError);
	void Resume();

	UPROPERTY(Transient)
	UStepAsset* Definition = nullptr;

	UPROPERTY(Transient)
	TMap<FString, UStepBase*> StepMap;

	UPROPERTY(Transient)
	TArray<UStepBase*> RootSteps;

	UPROPERTY(Transient)
	FStepCompiledGraph Graph;

	bool bActive = false;
};
