#pragma once

#include "Setting/SettingProviderBase.h"

#include "InputSettingProvider.generated.h"

UCLASS(BlueprintType, EditInlineNew)
class WHFRAMEWORK_API UInputSettingProvider : public USettingProviderBase
{
	GENERATED_BODY()

public:
	virtual void CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const override;

	virtual bool CanHandle(const FSettingDefinition& InDefinition) const override;

	virtual FParameter GetAppliedValue(const FSettingDefinition& InDefinition) const override;

	virtual FParameter GetPendingValue(const FSettingDefinition& InDefinition) const override;

	virtual bool SetPendingValue(const FSettingDefinition& InDefinition, const FParameter& InValue) override;

	virtual bool Apply(const FSettingDefinition& InDefinition, const FParameter& InValue) override;

private:
	TMap<FSettingId, FKey> PendingKeys;
};
