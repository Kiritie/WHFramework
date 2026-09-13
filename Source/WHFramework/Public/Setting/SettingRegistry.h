#pragma once

#include "Engine/DataAsset.h"
#include "Setting/SettingModuleTypes.h"

#include "SettingRegistry.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API USettingRegistry : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Setting")
	void GenerateSnapshot();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GeneratedSnapshotVersion = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FSettingDefinition> FinalDefinitions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FSettingDefinitionOverride> Overrides;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FSettingPageDefinition> Pages;
};
