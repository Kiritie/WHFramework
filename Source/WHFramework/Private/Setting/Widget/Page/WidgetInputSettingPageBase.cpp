// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/Page/WidgetInputSettingPageBase.h"

#include "Asset/AssetModuleStatics.h"
#include "Input/InputModule.h"
#include "Input/Base/PlayerMappableKeyProfileBase.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/SettingSaveGame.h"
#include "Setting/SettingModuleTypes.h"
#include "Setting/Widget/Item/WidgetKeySettingItemBase.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetInputSettingPageBase::UWidgetInputSettingPageBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = FName("InputSettingPage");

	Title = FText::FromString(TEXT("输入"));
}

void UWidgetInputSettingPageBase::OnInitialize(UObject* InOwner)
{
	Super::OnInitialize(InOwner);
}

void UWidgetInputSettingPageBase::OnCreate(UObject* InOwner)
{
	Super::OnCreate(InOwner);

	for (const auto& Mapping : UInputModule::Get().GetAllMappablePlayerKeyMappings())
	{
		if (Mapping.GetMappingName() != NAME_None && !Mapping.GetDisplayName().IsEmpty())
		{
			if(!SettingItems.Contains(Mapping.GetMappingName()))
			{
				UWidgetSettingItemBase* SettingItem = CreateSubWidget<UWidgetKeySettingItemBase>({ Mapping.GetDisplayName() }, UAssetModuleStatics::GetStaticClass(FName("KeySettingItem")));
				AddSettingItem(Mapping.GetMappingName(), SettingItem, Mapping.GetDisplayCategory());
			}
		}
	}
}

void UWidgetInputSettingPageBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);
	
	for(auto& Iter1 : SettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetAllPlayerKeyMappingsByName(Iter1.Key);
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

	for(auto& Iter : SettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetAllPlayerKeyMappingsByName(Iter.Key);
		TArray<FParameter> Values = Iter.Value->GetValues();
		for(int32 i = 0; i < Mappings.Num(); i++)
		{
			if(*Values[i].GetStringValue() != Mappings[i].GetCurrentKey())
			{
				UInputModule::Get().AddOrUpdateCustomKeyBindings(Mappings[i].GetMappingName(), *Values[i].GetStringValue(), GetOwningLocalPlayer()->GetLocalPlayerIndex());
			}
		}
	}
}

void UWidgetInputSettingPageBase::OnReset()
{
	Super::OnReset();

	for(auto& Iter : SettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetAllPlayerKeyMappingsByName(Iter.Key);
		TArray<FParameter> Values = Iter.Value->GetValues();
		for(int32 i = 0; i < Mappings.Num(); i++)
		{
			if(*Values[i].GetStringValue() != Mappings[i].GetDefaultKey())
			{
				Values[i] = Mappings[i].GetDefaultKey().ToString();
			}
			Iter.Value->SetValues(Values);
		}
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
	for(auto& Iter : SettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetAllPlayerKeyMappingsByName(Iter.Key);
		TArray<FParameter> Values = Iter.Value->GetValues();
		for(int32 i = 0; i < Mappings.Num(); i++)
		{
			if(*Values[i].GetStringValue() != Mappings[i].GetCurrentKey())
			{
				return true;
			}
		}
	}
	return false;
}

bool UWidgetInputSettingPageBase::CanReset_Implementation() const
{
	for(auto& Iter : SettingItems)
	{
		TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetAllPlayerKeyMappingsByName(Iter.Key);
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

FSaveData* UWidgetInputSettingPageBase::GetDefaultSaveData() const
{
	return &USaveGameModuleStatics::GetSaveGame<USettingSaveGame>()->GetDefaultDataRef<FSettingModuleSaveData>().InputData;
}
