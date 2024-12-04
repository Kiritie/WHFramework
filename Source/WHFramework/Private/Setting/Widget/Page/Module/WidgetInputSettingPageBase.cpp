// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/Page/Module/WidgetInputSettingPageBase.h"

#include "Input/InputModule.h"
#include "Input/Base/PlayerMappableKeyProfileBase.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/SettingSaveGame.h"
#include "Setting/SettingModule.h"
#include "Setting/SettingModuleTypes.h"
#include "Setting/Widget/Item/WidgetKeySettingItemBase.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetInputSettingPageBase::UWidgetInputSettingPageBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Title = FText::FromString(TEXT("输入"));
}

void UWidgetInputSettingPageBase::OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);

	for (const auto& Iter : UInputModule::Get().GetKeyShortcuts())
	{
		if (Iter.Key.GetTagName() != NAME_None && !Iter.Value.DisplayName.IsEmpty() && !Iter.Value.Keys.IsEmpty())
		{
			if(!ShortcutSettingItems.Contains(Iter.Key.GetTagName()))
			{
				UWidgetSettingItemBase* SettingItem = UObjectPoolModuleStatics::SpawnObject<UWidgetKeySettingItemBase>(nullptr, { Iter.Value.DisplayName, Iter.Value.Keys.Num(), true }, USettingModule::Get().GetKeySettingItemClass());
				AddShortcutSettingItem(Iter.Key.GetTagName(), SettingItem, Iter.Value.Category);
			}
		}
	}

	for (const auto& Iter : UInputModule::Get().GetAllPlayerKeyMappings())
	{
		if (Iter.GetMappingName() != NAME_None && !Iter.GetDisplayName().IsEmpty())
		{
			if(!MappingSettingItems.Contains(Iter.GetMappingName()))
			{
				UWidgetSettingItemBase* SettingItem = UObjectPoolModuleStatics::SpawnObject<UWidgetKeySettingItemBase>(nullptr, { Iter.GetDisplayName(), 2, !Iter.GetMappingName().IsEqual(FName("SystemOperation")) }, USettingModule::Get().GetKeySettingItemClass());
				AddMappingSettingItem(Iter.GetMappingName(), SettingItem, Iter.GetDisplayCategory());
			}
		}
	}

	for(auto& Iter : ShortcutSettingItems)
	{
		FInputKeyShortcut& KeyShortcut = UInputModule::Get().GetKeyShortcuts()[FGameplayTag::RequestGameplayTag(Iter.Key)];
		TArray<FParameter> Values;
		for(auto& Iter2 : KeyShortcut.Keys)
		{
			Values.Add(Iter2);
		}
		Iter.Value->SetValues(Values);
	}

	for(auto& Iter1 : MappingSettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByName(Iter1.Key);
		TArray<FParameter> Values;
		for(auto& Iter2 : Mappings)
		{
			Values.Add(Iter2.GetCurrentKey());
		}
		Iter1.Value->SetValues(Values);
	}
}

void UWidgetInputSettingPageBase::OnApply()
{
	Super::OnApply();

	for(auto& Iter : ShortcutSettingItems)
	{
		FInputKeyShortcut& KeyShortcut = UInputModule::Get().GetKeyShortcuts()[FGameplayTag::RequestGameplayTag(Iter.Key)];
		TArray<FParameter> Values = Iter.Value->GetValues();
		for(int32 i = 0; i < Values.Num(); i++)
		{
			if(!KeyShortcut.Keys.IsValidIndex(i) || Values[i].GetKeyValue() != KeyShortcut.Keys[i])
			{
				KeyShortcut.Keys[i] = Values[i].GetKeyValue();
			}
		}
	}

	for(auto& Iter : MappingSettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByName(Iter.Key);
		TArray<FParameter> Values = Iter.Value->GetValues();
		for(int32 i = 0; i < Values.Num(); i++)
		{
			if(!Mappings.IsValidIndex(i) || Values[i].GetKeyValue() != Mappings[i].GetCurrentKey())
			{
				UInputModule::Get().AddPlayerKeyMapping(Iter.Key, Values[i].GetKeyValue(), i);
			}
		}
	}
}

void UWidgetInputSettingPageBase::OnActivated()
{
	Super::OnActivated();
}

void UWidgetInputSettingPageBase::OnDeactivated()
{
	Super::OnDeactivated();

	for(auto& Iter : ShortcutSettingItems)
	{
		FInputKeyShortcut& KeyShortcut = UInputModule::Get().GetKeyShortcuts()[FGameplayTag::RequestGameplayTag(Iter.Key)];
		TArray<FParameter> Values;
		for(auto& Iter2 : KeyShortcut.Keys)
		{
			Values.Add(Iter2);
		}
		Iter.Value->SetValues(Values);
	}

	for(auto& Iter1 : MappingSettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByName(Iter1.Key);
		TArray<FParameter> Values;
		for(auto& Iter2 : Mappings)
		{
			Values.Add(Iter2.GetCurrentKey());
		}
		Iter1.Value->SetValues(Values);
	}
}

void UWidgetInputSettingPageBase::OnReset(bool bForce)
{
	Super::OnReset(bForce);

	for(auto& Iter : ShortcutSettingItems)
	{
		FInputKeyShortcut& KeyShortcut = GetDefaultSaveData()->CastRef<FInputModuleSaveData>().KeyShortcuts[FGameplayTag::RequestGameplayTag(Iter.Key)];
		TArray<FParameter> Values;
		for(auto& Iter2 : KeyShortcut.Keys)
		{
			Values.Add(Iter2);
		}
		Iter.Value->SetValues(Values);
	}

	for(auto& Iter : MappingSettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByName(Iter.Key);
		TArray<FParameter> Values;
		for(auto& Iter2 : Mappings)
		{
			Values.Add(Iter2.GetDefaultKey());
		}
		Iter.Value->SetValues(Values);
	}
}

bool UWidgetInputSettingPageBase::CanApply_Implementation() const
{
	for(auto& Iter : ShortcutSettingItems)
	{
		FInputKeyShortcut& KeyShortcut = UInputModule::Get().GetKeyShortcuts()[FGameplayTag::RequestGameplayTag(Iter.Key)];
		TArray<FParameter> Values = Iter.Value->GetValues();
		for(int32 i = 0; i < Values.Num(); i++)
		{
			if(!KeyShortcut.Keys.IsValidIndex(i))
			{
				if(Values[i].GetKeyValue().IsValid())
				{
					return true;
				}
			}
			else if(Values[i].GetKeyValue() != KeyShortcut.Keys[i])
			{
				return true;
			}
		}
	}
	
	for(auto& Iter : MappingSettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByName(Iter.Key);
		TArray<FParameter> Values = Iter.Value->GetValues();
		for(int32 i = 0; i < Values.Num(); i++)
		{
			if(!Mappings.IsValidIndex(i))
			{
				if(Values[i].GetKeyValue().IsValid())
				{
					return true;
				}
			}
			else if(Values[i].GetKeyValue() != Mappings[i].GetCurrentKey())
			{
				return true;
			}
		}
	}
	return false;
}

bool UWidgetInputSettingPageBase::CanReset_Implementation() const
{
	for(auto& Iter : ShortcutSettingItems)
	{
		FInputKeyShortcut& KeyShortcut1 = UInputModule::Get().GetKeyShortcuts()[FGameplayTag::RequestGameplayTag(Iter.Key)];
		FInputKeyShortcut& KeyShortcut2 = GetDefaultSaveData()->CastRef<FInputModuleSaveData>().KeyShortcuts[FGameplayTag::RequestGameplayTag(Iter.Key)];
		for(int32 i = 0; i < KeyShortcut1.Keys.Num(); i++)
		{
			if(KeyShortcut2.Keys.IsValidIndex(i) && KeyShortcut1.Keys[i] != KeyShortcut2.Keys[i])
			{
				return true;
			}
		}
	}
	
	for(auto& Iter : MappingSettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByName(Iter.Key);
		for(int32 i = 0; i < Mappings.Num(); i++)
		{
			if(Mappings[i].GetCurrentKey() != Mappings[i].GetDefaultKey())
			{
				return true;
			}
		}
	}
	return false;
}

void UWidgetInputSettingPageBase::AddShortcutSettingItem_Implementation(const FName InName, UWidgetSettingItemBase* InSettingItem, const FText& InCategory)
{
	AddSettingItem(InName, InSettingItem, InCategory);

	ShortcutSettingItems.Add(InName, InSettingItem);
}

void UWidgetInputSettingPageBase::AddMappingSettingItem_Implementation(const FName InName, UWidgetSettingItemBase* InSettingItem, const FText& InCategory)
{
	AddSettingItem(InName, InSettingItem, InCategory);

	MappingSettingItems.Add(InName, InSettingItem);
}

void UWidgetInputSettingPageBase::ClearSettingItems_Implementation()
{
	Super::ClearSettingItems_Implementation();

	ShortcutSettingItems.Empty();
	MappingSettingItems.Empty();
}

FSaveData* UWidgetInputSettingPageBase::GetDefaultSaveData() const
{
	return &USaveGameModuleStatics::GetOrCreateSaveGame<USettingSaveGame>()->GetDefaultDataRef<FSettingModuleSaveData>().InputData;
}
