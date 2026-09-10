// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetEnumSettingItemBase.h"

#include "Common/CommonModuleStatics.h"
#include "Components/ComboBoxString.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetEnumSettingItemBase::UWidgetEnumSettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UWidgetEnumSettingItemBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);

	ComboBox_Value->OnSelectionChanged.AddDynamic(this, &UWidgetEnumSettingItemBase::OnComboBoxSelectionChanged);
	
	if(InParams.IsValidIndex(1))
	{
		if(InParams[1].Is<FString>())
		{
			for(int32 i = 0; i < UCommonModuleStatics::GetEnumItemNum(InParams[1].Get<FString>()); i++)
			{
				EnumNames.Add(UCommonModuleStatics::GetEnumDisplayNameByValue(InParams[1].Get<FString>(), i).ToString());
			}
		}
		else
		{
			if(const FStringArrayParameterValue* Value = InParams[1].GetStructPtr<FStringArrayParameterValue>())
			{
				EnumNames = Value->Value;
			}
		}
	}
	TArray<int32> IgnoreEnumIndexs;
	if(InParams.IsValidIndex(2))
	{
		if(const FInt32ArrayParameterValue* Value = InParams[2].GetStructPtr<FInt32ArrayParameterValue>())
		{
			IgnoreEnumIndexs = Value->Value;
		}
	}
	for(int32 i = 0; i < EnumNames.Num(); i++)
	{
		if(!IgnoreEnumIndexs.Contains(i))
		{
			ComboBox_Value->AddOption(EnumNames[i]);
		}
	}
}

void UWidgetEnumSettingItemBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	EnumNames.Empty();
	
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
	return EnumNames.Find(ComboBox_Value->GetSelectedOption());
}

void UWidgetEnumSettingItemBase::SetValue(const FParameter& InValue)
{
	ComboBox_Value->SetSelectedOption(EnumNames[FMath::Clamp(InValue.Get<int32>(), 0, EnumNames.Num() - 1)]);
	Super::SetValue(InValue);
}
