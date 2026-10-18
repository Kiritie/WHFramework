#pragma once

#include "Camera/CameraModuleTypes.h"
#include "Setting/Provider/SettingProviderBase.h"
#include "CameraSettingProvider.generated.h"

class UCameraModule;

UCLASS()
class WHFRAMEWORK_API UCameraSettingProvider : public USettingProviderBase
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
	FCameraUserSettings AppliedSettings;
	FCameraUserSettings PendingSettings;
	FCameraUserSettings DefaultSettings;
	FCameraConfig Config;

	UCameraModule* GetCameraModule() const;
	FParameter ReadValue(const FCameraUserSettings& InSettings, const FSettingDefinition& InDefinition) const;
	bool WriteValue(FCameraUserSettings& InOutSettings, const FSettingDefinition& InDefinition, const FParameter& InValue) const;
	static void ResolveDisplay(const FCameraConfig& InConfig, FName InProperty, float& OutScale, float& OutOffset);
};
