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
	Title = NSLOCTEXT("WH.WidgetInputSettingPageBase", "Input", "输入");
}

void UWidgetInputSettingPageBase::OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);

	for(const FGameplayTag& ActionTag : UInputModule::Get().GetAllMappableActions())
	{
		const TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByTag(ActionTag);
		if(!Mappings.IsEmpty() && !Mappings[0].GetDisplayName().IsEmpty())
		{
			const FName ActionName = ActionTag.GetTagName();
			if(!MappingSettingItems.Contains(ActionName))
			{
				UWidgetSettingItemBase* SettingItem = UObjectPoolModuleStatics::SpawnObject<UWidgetKeySettingItemBase>(FWidgetKeySettingItemSpawnParameter(Mappings[0].GetDisplayName(), 2, ActionTag != GameplayTags::Input_SystemOperation), USettingModule::Get().GetKeySettingItemClass());
				AddMappingSettingItem(ActionName, SettingItem, Mappings[0].GetDisplayCategory());
			}
		}
	}

	for(auto& Iter1 : MappingSettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByTag(FGameplayTag::RequestGameplayTag(Iter1.Key));
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

	for(auto& Iter : MappingSettingItems)
	{
		const FGameplayTag ActionTag = FGameplayTag::RequestGameplayTag(Iter.Key);
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByTag(ActionTag);
		TArray<FParameter> Values = Iter.Value->GetValues();
		for(int32 i = 0; i < Values.Num(); i++)
		{
			if(!Mappings.IsValidIndex(i) || Values[i].Get<FKey>() != Mappings[i].GetCurrentKey())
			{
				UInputModule::Get().MapPlayerKeyByTag(ActionTag, Values[i].Get<FKey>(), static_cast<EPlayerMappableKeySlot>(i));
			}
		}
	}
}

void UWidgetInputSettingPageBase::NativeOnActivated()
{
	Super::NativeOnActivated();
}

void UWidgetInputSettingPageBase::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	for(auto& Iter1 : MappingSettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByTag(FGameplayTag::RequestGameplayTag(Iter1.Key));
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

	for(auto& Iter : MappingSettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByTag(FGameplayTag::RequestGameplayTag(Iter.Key));
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
	for(auto& Iter : MappingSettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByTag(FGameplayTag::RequestGameplayTag(Iter.Key));
		TArray<FParameter> Values = Iter.Value->GetValues();
		for(int32 i = 0; i < Values.Num(); i++)
		{
			if(!Mappings.IsValidIndex(i))
			{
				if(Values[i].Get<FKey>().IsValid())
				{
					return true;
				}
			}
			else if(Values[i].Get<FKey>() != Mappings[i].GetCurrentKey())
			{
				return true;
			}
		}
	}
	return false;
}

bool UWidgetInputSettingPageBase::CanReset_Implementation() const
{
	for(auto& Iter : MappingSettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByTag(FGameplayTag::RequestGameplayTag(Iter.Key));
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

void UWidgetInputSettingPageBase::AddMappingSettingItem_Implementation(const FName InName, UWidgetSettingItemBase* InSettingItem, const FText& InCategory)
{
	AddSettingItem(InName, InSettingItem, InCategory);

	MappingSettingItems.Add(InName, InSettingItem);
}

void UWidgetInputSettingPageBase::ClearSettingItems_Implementation()
{
	Super::ClearSettingItems_Implementation();

	MappingSettingItems.Empty();
}

FSaveData* UWidgetInputSettingPageBase::GetDefaultSaveData() const
{
	return &USaveGameModuleStatics::GetOrCreateSaveGame<USettingSaveGame>()->GetDefaultDataRef<FSettingModuleSaveData>().InputData;
}
