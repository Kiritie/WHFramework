// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/Page/WidgetInputSettingPageBase.h"

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
	WidgetName = FName("InputSettingPage");

	Title = FText::FromString(TEXT("输入"));
}

void UWidgetInputSettingPageBase::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InOwner, InParams);
}

void UWidgetInputSettingPageBase::OnCreate(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);

	for (const auto& Iter : UInputModule::Get().GetKeyShortcuts())
	{
		if (Iter.Key != NAME_None && !Iter.Value.DisplayName.IsEmpty())
		{
			if(!ShortcutSettingItems.Contains(Iter.Key))
			{
				UWidgetSettingItemBase* SettingItem = CreateSubWidget<UWidgetKeySettingItemBase>({ Iter.Value.DisplayName, 1 }, USettingModule::Get().GetKeySettingItemClass());
				AddShortcutSettingItem(Iter.Key, SettingItem, Iter.Value.Category);
			}
		}
	}

	for (const auto& Iter : UInputModule::Get().GetAllPlayerKeyMappings())
	{
		if (Iter.GetMappingName() != NAME_None && !Iter.GetDisplayName().IsEmpty())
		{
			if(!MappingSettingItems.Contains(Iter.GetMappingName()))
			{
				UWidgetSettingItemBase* SettingItem = CreateSubWidget<UWidgetKeySettingItemBase>({ Iter.GetDisplayName(), 2 }, USettingModule::Get().GetKeySettingItemClass());
				AddMappingSettingItem(Iter.GetMappingName(), SettingItem, Iter.GetDisplayCategory());
			}
		}
	}
}

void UWidgetInputSettingPageBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);

	for(auto& Iter : ShortcutSettingItems)
	{
		FInputKeyShortcut& KeyShortcut = UInputModule::Get().GetKeyShortcuts()[Iter.Key];
		Iter.Value->SetValues({ KeyShortcut.Key.ToString() });
	}

	for(auto& Iter1 : MappingSettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByName(Iter1.Key);
		TArray<FParameter> Values;
		for(auto& Iter2 : Mappings)
		{
			Values.Add(Iter2.GetCurrentKey().ToString());
		}
		Iter1.Value->SetValues(Values);
	}
}

void UWidgetInputSettingPageBase::OnApply()
{
	Super::OnApply();

	for(auto& Iter : ShortcutSettingItems)
	{
		FInputKeyShortcut& KeyShortcut = UInputModule::Get().GetKeyShortcuts()[Iter.Key];
		TArray<FParameter> Values = Iter.Value->GetValues();
		KeyShortcut.Key = *Values[0].GetStringValue();
	}

	for(auto& Iter : MappingSettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByName(Iter.Key);
		TArray<FParameter> Values = Iter.Value->GetValues();
		for(int32 i = 0; i < Values.Num(); i++)
		{
			if(!Mappings.IsValidIndex(i) || *Values[i].GetStringValue() != Mappings[i].GetCurrentKey())
			{
				UInputModule::Get().AddOrUpdateCustomKeyBindings(Iter.Key, *Values[i].GetStringValue(), i);
			}
		}
	}
}

void UWidgetInputSettingPageBase::OnReset()
{
	Super::OnReset();

	for(auto& Iter : ShortcutSettingItems)
	{
		FInputKeyShortcut& KeyShortcut = GetDefaultSaveData()->CastRef<FInputModuleSaveData>().KeyShortcuts[Iter.Key];
		Iter.Value->SetValues({ KeyShortcut.Key.ToString() });
	}

	for(auto& Iter : MappingSettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByName(Iter.Key);
		TArray<FParameter> Values;
		for(auto& Iter2 : Mappings)
		{
			Values.Add(Iter2.GetDefaultKey().ToString());
		}
		Iter.Value->SetValues(Values);
	}
}

void UWidgetInputSettingPageBase::OnValuesChange(UWidgetSettingItemBase* InSettingItem, const TArray<FParameter>& InValues)
{
	Super::OnValuesChange(InSettingItem, InValues);
}

void UWidgetInputSettingPageBase::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);
}

bool UWidgetInputSettingPageBase::CanApply_Implementation() const
{
	for(auto& Iter : ShortcutSettingItems)
	{
		FInputKeyShortcut& KeyShortcut = UInputModule::Get().GetKeyShortcuts()[Iter.Key];
		TArray<FParameter> Values = Iter.Value->GetValues();
		if(!Values[0].GetStringValue().Equals(KeyShortcut.Key.ToString()))
		{
			return true;
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
				if(!Values[i].GetStringValue().Equals(TEXT("None")))
				{
					return true;
				}
			}
			else if(*Values[i].GetStringValue() != Mappings[i].GetCurrentKey())
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
		FInputKeyShortcut& KeyShortcut = GetDefaultSaveData()->CastRef<FInputModuleSaveData>().KeyShortcuts[Iter.Key];
		TArray<FParameter> Values = Iter.Value->GetValues();
		if(!Values[0].GetStringValue().Equals(KeyShortcut.Key.ToString()))
		{
			return true;
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
	return &USaveGameModuleStatics::GetSaveGame<USettingSaveGame>()->GetDefaultDataRef<FSettingModuleSaveData>().InputData;
}
