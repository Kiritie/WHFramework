// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/Page/Module/WidgetParameterSettingPageBase.h"

#include "Parameter/ParameterModule.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/SettingSaveGame.h"
#include "Setting/SettingModule.h"
#include "Setting/SettingModuleTypes.h"
#include "Setting/Widget/Item/WidgetBoolSettingItemBase.h"
#include "Setting/Widget/Item/WidgetKeySettingItemBase.h"
#include "Setting/Widget/Item/WidgetTextSettingItemBase.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetParameterSettingPageBase::UWidgetParameterSettingPageBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Title = NSLOCTEXT("WH.WidgetParameterSettingPageBase", "Parameters", "参数");
}

void UWidgetParameterSettingPageBase::OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);

	for (const auto& Iter : UParameterModule::Get().GetAllParameter())
	{
		if (Iter.Name != NAME_None && Iter.bRegistered && !Iter.Parameter.GetDescription().IsEmpty())
		{
			UWidgetSettingItemBase* SettingItem = nullptr;
			if (Iter.Parameter.Is<int32>() || Iter.Parameter.Is<float>() || Iter.Parameter.Is<FString>() ||
				Iter.Parameter.Is<FName>() || Iter.Parameter.Is<FText>())
			{
				SettingItem = UObjectPoolModuleStatics::SpawnObject<UWidgetTextSettingItemBase>(FWidgetSettingItemSpawnParameter(Iter.Parameter.GetDescription()), USettingModule::Get().GetTextSettingItemClass());
			}
			else if (Iter.Parameter.Is<bool>())
			{
				SettingItem = UObjectPoolModuleStatics::SpawnObject<UWidgetBoolSettingItemBase>(FWidgetSettingItemSpawnParameter(Iter.Parameter.GetDescription()), USettingModule::Get().GetBoolSettingItemClass());
			}
			else if (Iter.Parameter.Is<FKey>())
			{
				SettingItem = UObjectPoolModuleStatics::SpawnObject<UWidgetKeySettingItemBase>(FWidgetKeySettingItemSpawnParameter(Iter.Parameter.GetDescription(), 1, true), USettingModule::Get().GetKeySettingItemClass());
			}
			AddSettingItem(Iter.Name, SettingItem, Iter.Category);
		}
	}

	for(auto& Iter : SettingItems)
	{
		FParameter Parameter = UParameterModule::Get().GetParameter(Iter.Key);
		Iter.Value->SetValue(Parameter);
	}
}

void UWidgetParameterSettingPageBase::OnApply()
{
	Super::OnApply();

	for(auto& Iter : SettingItems)
	{
		FParameter Parameter = UParameterModule::Get().GetParameter(Iter.Key);
		FParameter Value = Iter.Value->GetValue();
		if(Value != Parameter)
		{
			UParameterModule::Get().SetParameter(Iter.Key, Value);
		}
	}
}

void UWidgetParameterSettingPageBase::NativeOnActivated()
{
	Super::NativeOnActivated();
}

void UWidgetParameterSettingPageBase::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	for(auto& Iter : SettingItems)
	{
		FParameter Parameter = UParameterModule::Get().GetParameter(Iter.Key);
		Iter.Value->SetValue(Parameter);
	}
}

void UWidgetParameterSettingPageBase::OnReset(bool bForce)
{
	Super::OnReset(bForce);
	
	for(auto& Iter : SettingItems)
	{
		FParameter Parameter = GetDefaultSaveData()->CastRef<FParameterModuleSaveData>().ParameterSets.GetParameter(Iter.Key);
		Iter.Value->SetValue(Parameter);
	}
}

bool UWidgetParameterSettingPageBase::CanApply_Implementation() const
{
	for(auto& Iter : SettingItems)
	{
		FParameter Parameter = UParameterModule::Get().GetParameter(Iter.Key);
		FParameter Value = Iter.Value->GetValue();
		if(Value != Parameter)
		{
			return true;
		}
	}
	return false;
}

bool UWidgetParameterSettingPageBase::CanReset_Implementation() const
{
	for(auto& Iter : SettingItems)
	{
		FParameter Parameter1 = UParameterModule::Get().GetParameter(Iter.Key);
		FParameter Parameter2 = GetDefaultSaveData()->CastRef<FParameterModuleSaveData>().ParameterSets.GetParameter(Iter.Key);
		if(Parameter1 != Parameter2)
		{
			return true;
		}
	}
	return false;
}

FSaveData* UWidgetParameterSettingPageBase::GetDefaultSaveData() const
{
	return &USaveGameModuleStatics::GetOrCreateSaveGame<USettingSaveGame>()->GetDefaultDataRef<FSettingModuleSaveData>().ParameterData;
}
