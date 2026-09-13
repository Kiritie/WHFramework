#pragma once

#include "Setting/Provider/SettingProviderBase.h"

#include "ResolutionSettingProvider.generated.h"

UCLASS(BlueprintType, EditInlineNew)
class WHFRAMEWORK_API UResolutionSettingProvider : public USettingProviderBase
{
	GENERATED_BODY()

public:
	virtual void CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const override;
};
