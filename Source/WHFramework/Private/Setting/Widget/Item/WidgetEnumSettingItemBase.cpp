// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetEnumSettingItemBase.h"

#include "Common/CommonModuleStatics.h"
#include "Components/ComboBoxString.h"
#include "Setting/SettingModuleTypes.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetEnumSettingItemBase::UWidgetEnumSettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UWidgetEnumSettingItemBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);

	ComboBox_Value->OnSelectionChanged.AddDynamic(this, &UWidgetEnumSettingItemBase::OnComboBoxSelectionChanged);
	
	TArray<int32> IgnoredIndices;
	const FWidgetEnumSettingItemSpawnParameter& Parameter = InParam.GetRef<FWidgetEnumSettingItemSpawnParameter>();
	if(!Parameter.EnumNames.IsEmpty())
	{
		EnumNames = Parameter.EnumNames;
	}
	else if(!Parameter.EnumName.IsEmpty())
	{
		for(int32 Index = 0; Index < UCommonModuleStatics::GetEnumItemNum(Parameter.EnumName); ++Index)
		{
			EnumNames.Add(UCommonModuleStatics::GetEnumDisplayNameByValue(Parameter.EnumName, Index).ToString());
		}
	}
	IgnoredIndices = Parameter.IgnoredIndices;
	for(int32 Index = 0; Index < EnumNames.Num(); ++Index)
	{
		if(!IgnoredIndices.Contains(Index))
		{
			ComboBox_Value->AddOption(EnumNames[Index]);
		}
	}
}

void UWidgetEnumSettingItemBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);

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
