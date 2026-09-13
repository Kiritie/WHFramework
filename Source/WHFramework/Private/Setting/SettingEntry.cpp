#include "Setting/SettingEntry.h"

#include "Setting/SettingModule.h"

void USettingEntry::Initialize(USettingModule* InOwner, const FSettingDefinition& InDefinition)
{
	OwnerModule = InOwner;
	Definition = InDefinition;
}

FParameter USettingEntry::GetAppliedValue() const
{
	return OwnerModule ? OwnerModule->GetAppliedValue(Definition.SettingId) : FParameter();
}

FParameter USettingEntry::GetPendingValue() const
{
	return OwnerModule ? OwnerModule->GetPendingValue(Definition.SettingId) : FParameter();
}

FParameter USettingEntry::GetDefaultValue() const
{
	return OwnerModule ? OwnerModule->GetDefaultValue(Definition.SettingId) : FParameter();
}

bool USettingEntry::SetPendingValue(const FParameter& InValue)
{
	return OwnerModule && OwnerModule->SetPendingValue(Definition.SettingId, InValue);
}

bool USettingEntry::IsDirty() const
{
	return OwnerModule && OwnerModule->IsSettingDirty(Definition.SettingId);
}

bool USettingEntry::CanReset() const
{
	return OwnerModule && OwnerModule->CanResetSetting(Definition.SettingId);
}

bool USettingEntry::IsEnabled() const
{
	return OwnerModule && OwnerModule->IsSettingEnabled(Definition.SettingId);
}

bool USettingEntry::IsVisible() const
{
	return OwnerModule && OwnerModule->IsSettingVisible(Definition.SettingId);
}

FSettingValidationResult USettingEntry::GetValidationResult() const
{
	return OwnerModule ? OwnerModule->GetValidationResult(Definition.SettingId) : FSettingValidationResult::Valid();
}
