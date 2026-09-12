#include "Setting/SettingProviderBase.h"

void USettingProviderBase::CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const
{
}

bool USettingProviderBase::CanHandle(const FSettingDefinition& InDefinition) const
{
	return false;
}

FParameter USettingProviderBase::GetAppliedValue(const FSettingDefinition& InDefinition) const
{
	return FParameter();
}

FParameter USettingProviderBase::GetPendingValue(const FSettingDefinition& InDefinition) const
{
	return GetAppliedValue(InDefinition);
}

bool USettingProviderBase::SetPendingValue(const FSettingDefinition& InDefinition, const FParameter& InValue)
{
	return false;
}

bool USettingProviderBase::Preview(const FSettingDefinition& InDefinition, const FParameter& InValue)
{
	return false;
}

bool USettingProviderBase::Apply(const FSettingDefinition& InDefinition, const FParameter& InValue)
{
	return false;
}

void USettingProviderBase::Rollback(const FSettingDefinition& InDefinition)
{
}
