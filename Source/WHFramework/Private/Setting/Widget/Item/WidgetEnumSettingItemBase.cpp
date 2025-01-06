// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetEnumSettingItemBase.h"

#include "Common/CommonStatics.h"
#include "Components/ComboBoxString.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetEnumSettingItemBase::UWidgetEnumSettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UWidgetEnumSettingItemBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);

	ComboBox_Value->OnSelectionChanged.AddDynamic(this, &UWidgetEnumSettingItemBase::OnComboBoxSelectionChanged);
	
	FString EnumName;
	TArray<FString> EnumNames;
	int32 EnumMaxNum = -1;

	if(InParams.IsValidIndex(1))
	{
		if(InParams[1].GetParameterType() == EParameterType::String)
		{
			EnumName = InParams[1].GetStringValue();
		}
		else
		{
			EnumNames = InParams[1].GetPointerValueRef<TArray<FString>>();
		}
	}
	if(InParams.IsValidIndex(2))
	{
		EnumMaxNum = InParams[2];
	}
	
	if(!EnumName.IsEmpty())
	{
		for(int32 i = 0; i < (EnumMaxNum != -1 ? EnumMaxNum : UCommonStatics::GetEnumItemNum(EnumName)); i++)
		{
			ComboBox_Value->AddOption(UCommonStatics::GetEnumDisplayNameByValue(EnumName, i).ToString());
		}
	}
	else
	{
		for(int32 i = 0; i < (EnumMaxNum != -1 ? EnumMaxNum : EnumNames.Num()); i++)
		{
			ComboBox_Value->AddOption(EnumNames[i]);
		}
	}
}

void UWidgetEnumSettingItemBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	ComboBox_Value->ClearOptions();

	ComboBox_Value->OnSelectionChanged.RemoveDynamic(this, &UWidgetEnumSettingItemBase::OnComboBoxSelectionChanged);
}

void UWidgetEnumSettingItemBase::OnRefresh()
{
	Super::OnRefresh();
}

void UWidgetEnumSettingItemBase::OnComboBoxSelectionChanged(FString InSelectedItem, ESelectInfo::Type InSelectionType)
{
	if(OnValueChanged.IsBound())
	{
		OnValueChanged.Broadcast(this, GetValue());
	}
	Refresh();
}

FParameter UWidgetEnumSettingItemBase::GetValue() const
{
	return ComboBox_Value->GetSelectedIndex();
}

void UWidgetEnumSettingItemBase::SetValue(const FParameter& InValue)
{
	ComboBox_Value->SetSelectedIndex(InValue);
	Super::SetValue(InValue);
}
