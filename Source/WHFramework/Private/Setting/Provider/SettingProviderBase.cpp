#include "Setting/Provider/SettingProviderBase.h"

#include "Setting/SettingModule.h"

UWorld* USettingProviderBase::GetWorld() const
{
	const USettingModule* Owner = GetTypedOuter<USettingModule>();
	return Owner ? Owner->GetWorld() : nullptr;
}

void USettingProviderBase::CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const
{
}

bool USettingProviderBase::CanHandle(const FSettingDefinition& InDefinition) const
{
	return false;
}

void USettingProviderBase::BeginEdit(const TArray<FSettingDefinition>& InDefinitions)
{
}

FParameter USettingProviderBase::GetAppliedValue(const FSettingDefinition& InDefinition) const
{
	return FParameter();
}

FParameter USettingProviderBase::GetPendingValue(const FSettingDefinition& InDefinition) const
{
	return GetAppliedValue(InDefinition);
}

FParameter USettingProviderBase::GetDefaultValue(const FSettingDefinition& InDefinition) const
{
	return FParameter();
}

FSettingValidationResult USettingProviderBase::Validate(const FSettingDefinition& InDefinition, const FParameter& InValue) const
{
	return FSettingValidationResult::Valid();
}

bool USettingProviderBase::SetPendingValue(const FSettingDefinition& InDefinition, const FParameter& InValue)
{
	return false;
}

bool USettingProviderBase::Preview(const FSettingDefinition& InDefinition, const FParameter& InValue)
{
	return Apply(InDefinition, InValue);
}

bool USettingProviderBase::Apply(const FSettingDefinition& InDefinition, const FParameter& InValue)
{
	return false;
}

void USettingProviderBase::Rollback(const FSettingDefinition& InDefinition)
{
}

void USettingProviderBase::Commit()
{
}

void USettingProviderBase::EndEdit()
{
}
