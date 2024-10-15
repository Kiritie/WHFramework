// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/Page/WidgetSettingPageBase.h"

#include "Asset/AssetModuleStatics.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Setting/SettingModule.h"
#include "Setting/Widget/Item/WidgetSettingItemBase.h"
#include "Setting/Widget/Item/WidgetSettingItemCategoryBase.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetSettingPageBase::UWidgetSettingPageBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ParentName = FName("SettingPanel");
	ParentSlot = FName("Slot_SettingPage");
	WidgetType = EWidgetType::Temporary;
	WidgetInputMode = EInputMode::UIOnly;
	WidgetCreateType = EWidgetCreateType::AutoCreate;
	WidgetCloseType = EWidgetCloseType::Remove;
}

void UWidgetSettingPageBase::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InOwner, InParams);
}

void UWidgetSettingPageBase::OnCreate(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);

	if(!ContentBox)
	{
		ContentBox = NewObject<UVerticalBox>(this, UVerticalBox::StaticClass(), FName("ContentBox"));
	}

	WidgetTree->RootWidget = ContentBox;
}

void UWidgetSettingPageBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);
}

void UWidgetSettingPageBase::OnReset(bool bForce)
{
	Super::OnReset(bForce);
}

void UWidgetSettingPageBase::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);
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
		if(UVerticalBoxSlot* VerticalBoxSlot = ContentBox->AddChildToVerticalBox(UObjectPoolModuleStatics::SpawnObject<UWidgetSettingItemCategoryBase>(nullptr, { InCategory }, false, USettingModule::Get().GetSettingItemCategoryClass())))
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
