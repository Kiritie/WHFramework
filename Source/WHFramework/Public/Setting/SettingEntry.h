#pragma once

#include "Setting/SettingModuleTypes.h"

#include "SettingEntry.generated.h"

class USettingModule;

UCLASS(BlueprintType)
class WHFRAMEWORK_API USettingEntry : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USettingModule* InOwner, const FSettingDefinition& InDefinition);

	UFUNCTION(BlueprintPure)
	const FSettingDefinition& GetDefinition() const { return Definition; }

	UFUNCTION(BlueprintPure)
	FParameter GetAppliedValue() const;

	UFUNCTION(BlueprintPure)
	FParameter GetPendingValue() const;

	UFUNCTION(BlueprintPure)
	FParameter GetDefaultValue() const;

	UFUNCTION(BlueprintCallable)
	bool SetPendingValue(const FParameter& InValue);

	UFUNCTION(BlueprintPure)
	bool IsDirty() const;

	UFUNCTION(BlueprintPure)
	bool CanReset() const;

	UFUNCTION(BlueprintPure)
	bool IsEnabled() const;

	UFUNCTION(BlueprintPure)
	bool IsVisible() const;

	UFUNCTION(BlueprintPure)
	FSettingValidationResult GetValidationResult() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<USettingModule> OwnerModule;

	UPROPERTY(Transient)
	FSettingDefinition Definition;
};
