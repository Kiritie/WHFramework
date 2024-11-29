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
	Title = FText::FromString(TEXT("参数"));
}

void UWidgetParameterSettingPageBase::OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);

	for (const auto& Iter : UParameterModule::Get().GetAllParameter())
	{
		if (Iter.Name != NAME_None && Iter.bRegistered && !Iter.Parameter.GetDescription().IsEmpty())
		{
			UWidgetSettingItemBase* SettingItem = nullptr;
			switch (Iter.Parameter.GetParameterType())
			{
				case EParameterType::Integer:
				case EParameterType::Float:
				case EParameterType::String:
				case EParameterType::Name:
				case EParameterType::Text:
				{
					SettingItem = UObjectPoolModuleStatics::SpawnObject<UWidgetTextSettingItemBase>(nullptr, { Iter.Parameter.GetDescription() }, USettingModule::Get().GetTextSettingItemClass());
					break;
				}
				case EParameterType::Boolean:
				{
					SettingItem = UObjectPoolModuleStatics::SpawnObject<UWidgetBoolSettingItemBase>(nullptr, { Iter.Parameter.GetDescription() }, USettingModule::Get().GetBoolSettingItemClass());
					break;
				}
				case EParameterType::Key:
				{
					SettingItem = UObjectPoolModuleStatics::SpawnObject<UWidgetKeySettingItemBase>(nullptr, { Iter.Parameter.GetDescription(), 1, true }, USettingModule::Get().GetKeySettingItemClass());
					break;
				}
				default: break;
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

void UWidgetParameterSettingPageBase::OnActivated()
{
	Super::OnActivated();
}

void UWidgetParameterSettingPageBase::OnDeactivated()
{
	Super::OnDeactivated();

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
		FParameter Parameter = GetDefaultSaveData()->CastRef<FParameterModuleSaveData>().Parameters.GetParameter(Iter.Key);
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
		FParameter Parameter2 = GetDefaultSaveData()->CastRef<FParameterModuleSaveData>().Parameters.GetParameter(Iter.Key);
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
