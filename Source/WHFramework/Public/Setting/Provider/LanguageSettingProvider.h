#pragma once

#include "Setting/Provider/SettingProviderBase.h"

#include "LanguageSettingProvider.generated.h"

UCLASS(BlueprintType, EditInlineNew)
class WHFRAMEWORK_API ULanguageSettingProvider : public USettingProviderBase
{
	GENERATED_BODY()

public:
	virtual void CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const override;
};
