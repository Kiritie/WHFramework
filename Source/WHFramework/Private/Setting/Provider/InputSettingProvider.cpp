#include "Setting/Provider/InputSettingProvider.h"

#include "Input/InputModule.h"

namespace
{
	const FName InputProviderName(TEXT("Input"));

	FString GetSlotName(EPlayerMappableKeySlot InSlot)
	{
		switch(InSlot)
		{
			case EPlayerMappableKeySlot::First:
				return TEXT("Primary");
			case EPlayerMappableKeySlot::Second:
				return TEXT("Secondary");
			case EPlayerMappableKeySlot::Third:
				return TEXT("Tertiary");
			default:
				return FString::Printf(TEXT("Slot%d"), static_cast<int32>(InSlot));
		}
	}
}

void UInputSettingProvider::CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const
{
	for(const FInputMappableEntry& Entry : UInputModule::Get().GetAllMappableEntries())
	{
		const FSettingId SettingId{FName(*FString::Printf(TEXT("Input.%s.%s"), *Entry.MappingName.ToString(), *GetSlotName(Entry.Slot)))};
		FSettingDefinition* Definition = OutDefinitions.FindByPredicate([SettingId](const FSettingDefinition& Item)
		{
			return Item.SettingId == SettingId;
		});
		if(!Definition)
		{
			Definition = &OutDefinitions.AddDefaulted_GetRef();
		}

		Definition->SettingId = SettingId;
		Definition->SourcePath = Entry.MappingName.ToString();
		Definition->Provider = InputProviderName;
		Definition->Page = FName(TEXT("Input"));
		Definition->Category = Entry.DisplayCategory.IsEmpty() ? FName(TEXT("Bindings")) : FName(*Entry.DisplayCategory.ToString());
		Definition->Order = static_cast<int32>(Entry.Slot);
		Definition->DisplayName = Entry.DisplayName.IsEmpty() ? FText::FromName(Entry.MappingName) : Entry.DisplayName;
		Definition->Renderer = ESettingRendererType::Key;
		Definition->ApplyPolicy = ESettingApplyPolicy::Deferred;
		Definition->SemanticTag = Entry.ActionTag;
	}
}

bool UInputSettingProvider::CanHandle(const FSettingDefinition& InDefinition) const
{
	return InDefinition.Provider == InputProviderName;
}

void UInputSettingProvider::BeginEdit(const TArray<FSettingDefinition>& InDefinitions)
{
	AppliedKeys.Reset();
	PendingKeys.Reset();
	DefaultKeys.Reset();
	for(const FSettingDefinition& Definition : InDefinitions)
	{
		if(!CanHandle(Definition))
		{
			continue;
		}

		const FKey AppliedKey = FindCurrentKey(Definition);
		const FKey DefaultKey = FindDefaultKey(Definition);
		AppliedKeys.Add(Definition.SettingId, AppliedKey);
		PendingKeys.Add(Definition.SettingId, AppliedKey);
		DefaultKeys.Add(Definition.SettingId, DefaultKey);
	}
}

FParameter UInputSettingProvider::GetAppliedValue(const FSettingDefinition& InDefinition) const
{
	if(const FKey* Key = AppliedKeys.Find(InDefinition.SettingId))
	{
		return FParameter(*Key);
	}
	return FParameter(FindCurrentKey(InDefinition));
}

FParameter UInputSettingProvider::GetPendingValue(const FSettingDefinition& InDefinition) const
{
	if(const FKey* Key = PendingKeys.Find(InDefinition.SettingId))
	{
		return FParameter(*Key);
	}
	return GetAppliedValue(InDefinition);
}

FParameter UInputSettingProvider::GetDefaultValue(const FSettingDefinition& InDefinition) const
{
	if(const FKey* Key = DefaultKeys.Find(InDefinition.SettingId))
	{
		return FParameter(*Key);
	}
	return FParameter(FindDefaultKey(InDefinition));
}

FSettingValidationResult UInputSettingProvider::Validate(const FSettingDefinition& InDefinition, const FParameter& InValue) const
{
	const FKey* Key = InValue.GetPtr<FKey>();
	if(!Key || !Key->IsValid())
	{
		return FSettingValidationResult::Invalid(NSLOCTEXT("WH.InputSettingProvider", "InvalidKey", "请选择有效的按键。"));
	}

	for(const FPlayerKeyMapping& Mapping : UInputModule::Get().GetAllPlayerKeyMappings())
	{
		if(Mapping.GetCurrentKey() == *Key && (Mapping.GetMappingName() != FName(*InDefinition.SourcePath) || Mapping.GetSlot() != static_cast<EPlayerMappableKeySlot>(InDefinition.Order)))
		{
			return FSettingValidationResult::Invalid(FText::Format(
				NSLOCTEXT("WH.InputSettingProvider", "KeyConflict", "按键 {0} 已被其他输入操作占用。"),
				Key->GetDisplayName()));
		}
	}
	return FSettingValidationResult::Valid();
}

bool UInputSettingProvider::SetPendingValue(const FSettingDefinition& InDefinition, const FParameter& InValue)
{
	const FKey* Key = InValue.GetPtr<FKey>();
	if(!Key)
	{
		return false;
	}
	PendingKeys.Add(InDefinition.SettingId, *Key);
	return true;
}

bool UInputSettingProvider::Apply(const FSettingDefinition& InDefinition, const FParameter& InValue)
{
	const FKey* Key = InValue.GetPtr<FKey>();
	if(!Key)
	{
		return false;
	}

	FGameplayTagContainer FailureReasons;
	return UInputModule::Get().MapPlayerKeyByMappingName(
		FName(*InDefinition.SourcePath),
		*Key,
		static_cast<EPlayerMappableKeySlot>(InDefinition.Order),
		0,
		&FailureReasons);
}

void UInputSettingProvider::Rollback(const FSettingDefinition& InDefinition)
{
	if(const FKey* Key = AppliedKeys.Find(InDefinition.SettingId))
	{
		UInputModule::Get().MapPlayerKeyByMappingName(
			FName(*InDefinition.SourcePath),
			*Key,
			static_cast<EPlayerMappableKeySlot>(InDefinition.Order));
		PendingKeys.Add(InDefinition.SettingId, *Key);
	}
}

void UInputSettingProvider::Commit()
{
	AppliedKeys = PendingKeys;
}

void UInputSettingProvider::EndEdit()
{
	AppliedKeys.Reset();
	PendingKeys.Reset();
	DefaultKeys.Reset();
}

FKey UInputSettingProvider::FindCurrentKey(const FSettingDefinition& InDefinition) const
{
	const EPlayerMappableKeySlot Slot = static_cast<EPlayerMappableKeySlot>(InDefinition.Order);
	const TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByMappingName(FName(*InDefinition.SourcePath));
	if(const FPlayerKeyMapping* Mapping = Mappings.FindByPredicate([Slot](const FPlayerKeyMapping& Item)
	{
		return Item.GetSlot() == Slot;
	}))
	{
		return Mapping->GetCurrentKey();
	}
	return FKey();
}

FKey UInputSettingProvider::FindDefaultKey(const FSettingDefinition& InDefinition) const
{
	const FName MappingName(*InDefinition.SourcePath);
	const EPlayerMappableKeySlot Slot = static_cast<EPlayerMappableKeySlot>(InDefinition.Order);
	const TArray<FInputMappableEntry> Entries = UInputModule::Get().GetAllMappableEntries();
	if(const FInputMappableEntry* Entry = Entries.FindByPredicate([MappingName, Slot](const FInputMappableEntry& Item)
	{
		return Item.MappingName == MappingName && Item.Slot == Slot;
	}))
	{
		return Entry->DefaultKey;
	}
	return FKey();
}
