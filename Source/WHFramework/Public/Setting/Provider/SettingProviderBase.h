#pragma once

#include "Setting/SettingModuleTypes.h"

#include "SettingProviderBase.generated.h"

UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class WHFRAMEWORK_API USettingProviderBase : public UObject
{
	GENERATED_BODY()

public:
	virtual void CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const;

	virtual bool CanHandle(const FSettingDefinition& InDefinition) const;

	virtual void BeginEdit(const TArray<FSettingDefinition>& InDefinitions);

	virtual FParameter GetAppliedValue(const FSettingDefinition& InDefinition) const;

	virtual FParameter GetPendingValue(const FSettingDefinition& InDefinition) const;

	virtual FParameter GetDefaultValue(const FSettingDefinition& InDefinition) const;

	virtual FSettingValidationResult Validate(const FSettingDefinition& InDefinition, const FParameter& InValue) const;

	virtual bool SetPendingValue(const FSettingDefinition& InDefinition, const FParameter& InValue);

	virtual bool Preview(const FSettingDefinition& InDefinition, const FParameter& InValue);

	virtual bool Apply(const FSettingDefinition& InDefinition, const FParameter& InValue);

	virtual void Rollback(const FSettingDefinition& InDefinition);

	virtual void Commit();

	virtual void EndEdit();
};
