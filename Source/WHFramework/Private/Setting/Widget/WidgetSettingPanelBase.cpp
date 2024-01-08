// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/WidgetSettingPanelBase.h"

#include "Setting/Widget/Page/WidgetSettingPageBase.h"
#include "Setting/Widget/Page/WidgetSettingPageItemBase.h"
#include "Widget/WidgetModuleStatics.h"
#include "Widget/Common/CommonButtonGroup.h"

UWidgetSettingPanelBase::UWidgetSettingPanelBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = FName("SettingPanel");

	WidgetType = EWidgetType::Temporary;
	WidgetInputMode = EInputMode::UIOnly;

	WidgetZOrder = 10;

	SetIsFocusable(true);
	
	PageItemGroup = nullptr;
	PageItemClass = nullptr;

	Btn_Apply = nullptr;
	Btn_Reset = nullptr;
}

void UWidgetSettingPanelBase::OnCreate(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);

	if(Btn_Apply)
	{
		Btn_Apply->OnClicked().AddUObject(this, &UWidgetSettingPanelBase::OnApplyButtonClicked);
	}
	if(Btn_Reset)
	{
		Btn_Reset->OnClicked().AddUObject(this, &UWidgetSettingPanelBase::OnResetButtonClicked);
	}

	PageItemGroup = UObjectPoolModuleStatics::SpawnObject<UCommonButtonGroup>();
	PageItemGroup->SetSelectionRequired(true);
	PageItemGroup->SetBroadcastOnDeselected(false);

	for(const auto Iter : ChildWidgets)
	{
		SpawnPageItem(Cast<UWidgetSettingPageBase>(Iter));
	}
}

void UWidgetSettingPanelBase::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InOwner, InParams);
}

void UWidgetSettingPanelBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);
	
	SetCurrentPage(0);
}

void UWidgetSettingPanelBase::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);
}

void UWidgetSettingPanelBase::OnApplyButtonClicked()
{
	if(UWidgetSettingPageBase* SettingPage = GetCurrentPage())
	{
		SettingPage->Apply();
	}
}

void UWidgetSettingPanelBase::OnResetButtonClicked()
{
	if(UWidgetSettingPageBase* SettingPage = GetCurrentPage())
	{
		SettingPage->Reset();
	}
}

UWidgetSettingPageItemBase* UWidgetSettingPanelBase::SpawnPageItem_Implementation(UWidgetSettingPageBase* InPage)
{
	if(UWidgetSettingPageItemBase* PageItem = UObjectPoolModuleStatics::SpawnObject<UWidgetSettingPageItemBase>(InPage, nullptr, PageItemClass))
	{
		PageItem->SetTitle(InPage->Title);
		PageItemGroup->AddWidget(PageItem);
		return PageItem;
	}
	return nullptr;
}

int32 UWidgetSettingPanelBase::GetCurrentPageIndex() const
{
	return PageItemGroup->GetSelectedButtonIndex();
}

UWidgetSettingPageBase* UWidgetSettingPanelBase::GetCurrentPage() const
{
	return GetChild<UWidgetSettingPageBase>(GetCurrentPageIndex());
}

void UWidgetSettingPanelBase::SetCurrentPage_Implementation(int32 InPageIndex)
{
	PageItemGroup->SelectButtonAtIndex(InPageIndex);
}
