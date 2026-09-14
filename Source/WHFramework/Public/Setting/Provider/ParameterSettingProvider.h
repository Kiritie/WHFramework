#pragma once

#include "Setting/Provider/SettingProviderBase.h"

#include "ParameterSettingProvider.generated.h"

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class WHFRAMEWORK_API UParameterSettingProvider : public USettingProviderBase
{
	GENERATED_BODY()

public:
	virtual void CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const override;

	virtual bool CanHandle(const FSettingDefinition& InDefinition) const override;

	virtual void BeginEdit(const TArray<FSettingDefinition>& InDefinitions) override;

	virtual FParameter GetAppliedValue(const FSettingDefinition& InDefinition) const override;

	virtual FParameter GetPendingValue(const FSettingDefinition& InDefinition) const override;

	virtual FParameter GetDefaultValue(const FSettingDefinition& InDefinition) const override;

	virtual bool SetPendingValue(const FSettingDefinition& InDefinition, const FParameter& InValue) override;

	virtual bool Apply(const FSettingDefinition& InDefinition, const FParameter& InValue) override;

	virtual void Rollback(const FSettingDefinition& InDefinition) override;

	virtual void Commit() override;

	virtual void EndEdit() override;

private:
	TMap<FSettingId, FParameter> AppliedValues;

	TMap<FSettingId, FParameter> PendingValues;

	TMap<FSettingId, FParameter> DefaultValues;

	TMap<FSettingId, FName> ParameterNames;
};
