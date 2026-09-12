#pragma once

#include "Engine/DataAsset.h"
#include "Setting/SettingModuleTypes.h"

#include "SettingRegistry.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API USettingRegistry : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FSettingDefinitionOverride> Overrides;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FSettingPageDefinition> Pages;
};
