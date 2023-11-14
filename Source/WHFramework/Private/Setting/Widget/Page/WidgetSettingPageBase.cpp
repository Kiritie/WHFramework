// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/Page/WidgetSettingPageBase.h"

#include "Asset/AssetModuleStatics.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Setting/SettingModule.h"
#include "Setting/Widget/Item/WidgetSettingItemBase.h"
#include "Setting/Widget/Item/WidgetSettingItemCategoryBase.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetSettingPageBase::UWidgetSettingPageBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ParentName = FName("SettingPanel");
	WidgetType = EWidgetType::Temporary;
	WidgetInputMode = EInputMode::UIOnly;
	WidgetCreateType = EWidgetCreateType::AutoCreate;
}

void UWidgetSettingPageBase::OnInitialize(UObject* InOwner)
{
	Super::OnInitialize(InOwner);
}

void UWidgetSettingPageBase::OnCreate(UObject* InOwner)
{
	Super::OnCreate(InOwner);
}

void UWidgetSettingPageBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);
}

void UWidgetSettingPageBase::OnReset()
{
	Super::OnReset();
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
	InSettingItem->OnValueChanged.RemoveAll(this);
	InSettingItem->OnValueChanged.AddDynamic(this, &UWidgetSettingPageBase::OnValueChange);
	InSettingItem->OnValuesChanged.RemoveAll(this);
	InSettingItem->OnValuesChanged.AddDynamic(this, &UWidgetSettingPageBase::OnValuesChange);
	if(!InCategory.IsEmpty() && !InCategory.EqualTo(LastCategory))
	{
		LastCategory = InCategory;
		if(UScrollBoxSlot* ScrollBoxSlot = Cast<UScrollBoxSlot>(ContentBox->AddChild(CreateSubWidget<UWidgetSettingItemCategoryBase>({ InCategory }, USettingModule::Get().GetSettingItemCategoryClass()))))
		{
			ScrollBoxSlot->SetPadding(FMargin(2.5f, 0.f));
		}
	}
	if(UScrollBoxSlot* ScrollBoxSlot = Cast<UScrollBoxSlot>(ContentBox->AddChild(InSettingItem)))
	{
		ScrollBoxSlot->SetPadding(FMargin(2.5f));
	}
	SettingItems.Add(InName, InSettingItem);
}

void UWidgetSettingPageBase::ClearSettingItems_Implementation()
{
	for(auto Iter : ContentBox->GetAllChildren())
	{
		if(UWidgetSettingItemBase* SettingItem = Cast<UWidgetSettingItemBase>(Iter))
		{
			SettingItem->Destroy(true);
		}
	}
	ContentBox->ClearChildren();
	SettingItems.Empty();
}
