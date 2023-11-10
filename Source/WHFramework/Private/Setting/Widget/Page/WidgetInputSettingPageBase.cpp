// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/Page/WidgetInputSettingPageBase.h"


#include "InputMappingContext.h"
#include "Asset/AssetModuleStatics.h"
#include "Input/InputModule.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/SettingSaveGame.h"
#include "Setting/SettingModuleTypes.h"
#include "Setting/Widget/Item/WidgetKeySettingItemBase.h"
#include "Widget/WidgetModuleStatics.h"

class USettingSaveGame;

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

	for (const auto& Mapping : UInputModule::Get().GetAllPlayerMappableActionKeyMappings())
	{
		if (Mapping.PlayerMappableOptions.Name != NAME_None && !Mapping.PlayerMappableOptions.DisplayName.IsEmpty())
		{
			UWidgetSettingItemBase* SettingItem = nullptr;
			for(auto Iter : SettingItems)
			{
				if(Iter->GetLabel().EqualTo(Mapping.PlayerMappableOptions.DisplayName))
				{
					SettingItem = Iter;
					break;
				}
			}
			if(!SettingItem)
			{
				SettingItem = CreateSubWidget<UWidgetKeySettingItemBase>({ Mapping.PlayerMappableOptions.DisplayName }, UAssetModuleStatics::GetStaticClass(FName("KeySettingItem")));
			}
			AddSettingItem(SettingItem, Mapping.PlayerMappableOptions.DisplayCategory);
		}
	}
}

void UWidgetInputSettingPageBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);
	
	for(auto& Iter1 : SettingItems)
	{
		TArray<FEnhancedActionKeyMapping> Mappings = UInputModule::Get().GetAllActionMappingByDisplayName(Iter1->GetLabel());
		TArray<FParameter> Values;
		for(auto& Iter2 : Mappings)
		{
			Values.Add(Iter2.Key.ToString());
		}
		Iter1->SetValues(Values);
	}
}

void UWidgetInputSettingPageBase::OnApply()
{
	Super::OnApply();

	for(auto& Iter : SettingItems)
	{
		TArray<FEnhancedActionKeyMapping> Mappings = UInputModule::Get().GetAllActionMappingByDisplayName(Iter->GetLabel());
		TArray<FParameter> Values = Iter->GetValues();
		for(int32 i = 0; i < Values.Num(); i++)
		{
			if(*Values[i].GetStringValue() != Mappings[i].Key)
			{
				UInputModule::Get().AddOrUpdateCustomKeyboardBindings(Mappings[i].PlayerMappableOptions.Name, *Values[i].GetStringValue(), GetOwningLocalPlayer()->GetLocalPlayerIndex());
			}
		}
	}
}

void UWidgetInputSettingPageBase::OnReset()
{
	Super::OnReset();

	for(auto& Iter : SettingItems)
	{
		TArray<FEnhancedActionKeyMapping> Mappings = GetDefaultSaveData()->CastRef<FInputModuleSaveData>().GetAllActionMappingByDisplayName(Iter->GetLabel());
		TArray<FParameter> Values = Iter->GetValues();
		for(int32 i = 0; i < Values.Num(); i++)
		{
			if(*Values[i].GetStringValue() != Mappings[i].Key)
			{
				Values[i] = Mappings[i].Key.ToString();
			}
			Iter->SetValues(Values);
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
		TArray<FEnhancedActionKeyMapping> Mappings = UInputModule::Get().GetAllActionMappingByDisplayName(Iter->GetLabel());
		TArray<FParameter> Values = Iter->GetValues();
		for(int32 i = 0; i < Values.Num(); i++)
		{
			if(*Values[i].GetStringValue() != Mappings[i].Key)
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
		TArray<FEnhancedActionKeyMapping> Mappings1 = UInputModule::Get().GetAllActionMappingByDisplayName(Iter->GetLabel());
		TArray<FEnhancedActionKeyMapping> Mappings2 = GetDefaultSaveData()->CastRef<FInputModuleSaveData>().GetAllActionMappingByDisplayName(Iter->GetLabel());
		for(int32 i = 0; i < Mappings1.Num(); i++)
		{
			if(Mappings1[i].Key != Mappings2[i].Key)
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
