#pragma once

#include "Setting/Provider/SettingProviderBase.h"

#include "InputSettingProvider.generated.h"

UCLASS(BlueprintType, EditInlineNew)
class WHFRAMEWORK_API UInputSettingProvider : public USettingProviderBase
{
	GENERATED_BODY()

public:
	virtual void CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const override;

	virtual bool CanHandle(const FSettingDefinition& InDefinition) const override;

	virtual void BeginEdit(const TArray<FSettingDefinition>& InDefinitions) override;

	virtual FParameter GetAppliedValue(const FSettingDefinition& InDefinition) const override;

	virtual FParameter GetPendingValue(const FSettingDefinition& InDefinition) const override;

	virtual FParameter GetDefaultValue(const FSettingDefinition& InDefinition) const override;

	virtual FSettingValidationResult Validate(const FSettingDefinition& InDefinition, const FParameter& InValue) const override;

	virtual bool SetPendingValue(const FSettingDefinition& InDefinition, const FParameter& InValue) override;

	virtual bool Apply(const FSettingDefinition& InDefinition, const FParameter& InValue) override;

	virtual void Rollback(const FSettingDefinition& InDefinition) override;

	virtual void Commit() override;

	virtual void EndEdit() override;

private:
	FKey FindCurrentKey(const FSettingDefinition& InDefinition) const;

	FKey FindDefaultKey(const FSettingDefinition& InDefinition) const;

	TMap<FSettingId, FKey> AppliedKeys;

	TMap<FSettingId, FKey> PendingKeys;

	TMap<FSettingId, FKey> DefaultKeys;
};
