#pragma once

#include "Audio/AudioModuleTypes.h"
#include "Setting/Provider/SettingProviderBase.h"

#include "AudioSettingProvider.generated.h"

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class WHFRAMEWORK_API UAudioSettingProvider : public USettingProviderBase
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
	float& ResolveVolume(FAudioModuleSaveData& InData, FName InSettingName) const;

	const float& ResolveVolume(const FAudioModuleSaveData& InData, FName InSettingName) const;

private:
	FAudioModuleSaveData AppliedData;

	FAudioModuleSaveData PendingData;

	FAudioModuleSaveData DefaultData;
};
