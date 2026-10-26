#include "Setting/Provider/InputSettingProvider.h"

#include "Input/InputModule.h"
#include "Common/CommonModuleTypes.h"

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

	EPlayerMappableKeySlot GetDefinitionSlot(const FSettingDefinition& InDefinition)
	{
		const FString Id = InDefinition.SettingId.Name.ToString();
		for (int32 Index = static_cast<int32>(EPlayerMappableKeySlot::First); Index <= static_cast<int32>(EPlayerMappableKeySlot::Seventh); ++Index)
		{
			const EPlayerMappableKeySlot Slot = static_cast<EPlayerMappableKeySlot>(Index);
			if (Id.EndsWith(TEXT(".") + GetSlotName(Slot))) return Slot;
		}
		return EPlayerMappableKeySlot::Unspecified;
	}
}

void UInputSettingProvider::CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const
{
	const UInputModule* Input = UInputModule::GetPtr(GetWorld());
	if (!Input) return;
	int32 DisplayOrder = 0;
	for(const FInputMappableEntry& Entry : Input->GetAllMappableEntries())
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
		const FString SourceCategory = Entry.DisplayCategory.ToString();
		const bool bSystemOperation = SourceCategory.Contains(TEXT("System")) || SourceCategory.Contains(TEXT("系统"));
		Definition->Category = bSystemOperation ? FName(TEXT("System Operation")) : FName(TEXT("Character Control"));
		Definition->CategoryDisplayName = bSystemOperation ? NSLOCTEXT("WH.Setting", "SystemInput", "系统操作") : NSLOCTEXT("WH.Setting", "CharacterInput", "角色控制");
		Definition->CategoryOrder = bSystemOperation ? 10 : 0;
		Definition->Order = DisplayOrder++;
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
	UInputModule* Input = UInputModule::GetPtr(GetWorld());
	if (!Input)
	{
		return FSettingValidationResult::Invalid(NSLOCTEXT("WH.InputSettingProvider", "Unavailable", "当前世界的输入设置服务不可用。"));
	}
	const FKey* Key = InValue.GetPtr<FKey>();
	if(!Key || !Key->IsValid())
	{
		return FSettingValidationResult::Invalid(NSLOCTEXT("WH.InputSettingProvider", "InvalidKey", "请选择有效的按键。"));
	}

	const auto Conflict = [Key]()
	{
		return FSettingValidationResult::Invalid(FText::Format(
			NSLOCTEXT("WH.InputSettingProvider", "KeyConflict", "按键 {0} 已被其他输入操作占用。"),
			Key->GetDisplayName()));
	};
	for (const TPair<FSettingId, FKey>& Pending : PendingKeys)
	{
		if (Pending.Key != InDefinition.SettingId && Pending.Value == *Key)
		{
			return Conflict();
		}
	}
	for(const FPlayerKeyMapping& Mapping : Input->GetAllPlayerKeyMappings())
	{
		const FSettingId MappingId{FName(*FString::Printf(TEXT("Input.%s.%s"), *Mapping.GetMappingName().ToString(), *GetSlotName(Mapping.GetSlot())))};
		if (!PendingKeys.Contains(MappingId) && MappingId != InDefinition.SettingId && Mapping.GetCurrentKey() == *Key)
		{
			return Conflict();
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
	UInputModule* Input = UInputModule::GetPtr(GetWorld());
	const FKey* Key = InValue.GetPtr<FKey>();
	if(!Input || !CanHandle(InDefinition) || !Key)
	{
		return false;
	}

	FGameplayTagContainer FailureReasons;
	const FScopedWorldContext WorldContext(GetWorld());
	return Input->MapPlayerKeyByMappingName(
		FName(*InDefinition.SourcePath),
		*Key,
		GetDefinitionSlot(InDefinition),
		0,
		&FailureReasons);
}

void UInputSettingProvider::Rollback(const FSettingDefinition& InDefinition)
{
	if(const FKey* Key = AppliedKeys.Find(InDefinition.SettingId))
	{
		Apply(InDefinition, FParameter(*Key));
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
	const UInputModule* Input = UInputModule::GetPtr(GetWorld());
	if (!Input) return FKey();
	const EPlayerMappableKeySlot Slot = GetDefinitionSlot(InDefinition);
	const TArray<FPlayerKeyMapping> Mappings = Input->GetPlayerKeyMappingsByMappingName(FName(*InDefinition.SourcePath));
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
	const UInputModule* Input = UInputModule::GetPtr(GetWorld());
	if (!Input) return FKey();
	const FName MappingName(*InDefinition.SourcePath);
	const EPlayerMappableKeySlot Slot = GetDefinitionSlot(InDefinition);
	const TArray<FInputMappableEntry> Entries = Input->GetAllMappableEntries();
	if(const FInputMappableEntry* Entry = Entries.FindByPredicate([MappingName, Slot](const FInputMappableEntry& Item)
	{
		return Item.MappingName == MappingName && Item.Slot == Slot;
	}))
	{
		return Entry->DefaultKey;
	}
	return FKey();
}
