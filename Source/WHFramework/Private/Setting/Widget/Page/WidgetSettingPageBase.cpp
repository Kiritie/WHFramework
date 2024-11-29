// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/Page/WidgetSettingPageBase.h"

#include "Asset/AssetModuleStatics.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Setting/SettingModule.h"
#include "Setting/Widget/Item/WidgetSettingItemBase.h"
#include "Setting/Widget/Item/WidgetSettingItemCategoryBase.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetSettingPageBase::UWidgetSettingPageBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UWidgetSettingPageBase::OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);

	if(!ContentBox)
	{
		ContentBox = NewObject<UVerticalBox>(this);
	}

	WidgetTree->RootWidget = ContentBox;
}

void UWidgetSettingPageBase::OnReset(bool bForce)
{
	Super::OnReset(bForce);
}

void UWidgetSettingPageBase::OnActivated()
{
	Super::OnActivated();
}

void UWidgetSettingPageBase::OnDeactivated()
{
	Super::OnDeactivated();
}

void UWidgetSettingPageBase::OnApply()
{
	K2_OnApply();
}

void UWidgetSettingPageBase::OnValueChange(UWidgetSettingItemBase* InSettingItem, const FParameter& InValue)
{
	K2_OnValueChange(InSettingItem, InValue);
}

void UWidgetSettingPageBase::OnValuesChange(UWidgetSettingItemBase* InSettingItem, const TArray<FParameter>& InValues)
{
	K2_OnValuesChange(InSettingItem, InValues);
}

bool UWidgetSettingPageBase::CanApply_Implementation() const
{
	return false;
}

bool UWidgetSettingPageBase::CanReset_Implementation() const
{
	return false;
}

void UWidgetSettingPageBase::Apply()
{
	OnApply();
}

FSaveData* UWidgetSettingPageBase::GetDefaultSaveData() const
{
	return nullptr;
}

void UWidgetSettingPageBase::AddSettingItem_Implementation(const FName InName, UWidgetSettingItemBase* InSettingItem, const FText& InCategory)
{
	if(!InSettingItem) return;
	
	InSettingItem->SetNameS(InName);
	InSettingItem->OnValueChanged.AddDynamic(this, &UWidgetSettingPageBase::OnValueChange);
	InSettingItem->OnValuesChanged.AddDynamic(this, &UWidgetSettingPageBase::OnValuesChange);
	if(!InCategory.IsEmpty() && !InCategory.EqualTo(LastCategory))
	{
		LastCategory = InCategory;
		if(UVerticalBoxSlot* VerticalBoxSlot = ContentBox->AddChildToVerticalBox(UObjectPoolModuleStatics::SpawnObject<UWidgetSettingItemCategoryBase>(nullptr, { InCategory }, USettingModule::Get().GetSettingItemCategoryClass())))
		{
			VerticalBoxSlot->SetPadding(FMargin(2.5f));
		}
	}
	if(UVerticalBoxSlot* VerticalBoxSlot = ContentBox->AddChildToVerticalBox(InSettingItem))
	{
		VerticalBoxSlot->SetPadding(FMargin(2.5f));
	}
	SettingItems.Add(InName, InSettingItem);
}

void UWidgetSettingPageBase::ClearSettingItems_Implementation()
{
	UObjectPoolModuleStatics::DespawnObjects(ContentBox->GetAllChildren());
	ContentBox->ClearChildren();
	SettingItems.Empty();
}

UWidgetSettingItemBase* UWidgetSettingPageBase::GetSettingItemByName(const FName InName)
{
	if(SettingItems.Contains(InName))
	{
		return SettingItems[InName];
	}
	return nullptr;
}
