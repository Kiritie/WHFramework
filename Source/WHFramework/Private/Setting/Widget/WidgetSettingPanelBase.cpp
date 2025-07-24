// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/WidgetSettingPanelBase.h"

#include "CommonActivatableWidgetSwitcher.h"
#include "Setting/Widget/Page/WidgetSettingPageBase.h"
#include "Widget/WidgetModuleStatics.h"
#include "Widget/Common/CommonButton.h"
#include "Widget/Common/CommonButtonGroup.h"

UWidgetSettingPanelBase::UWidgetSettingPanelBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = FName("SettingPanel");

	WidgetType = EWidgetType::Temporary;
	WidgetInputMode = EInputMode::UIOnly;

	WidgetZOrder = 10;

	bWidgetAutoFocus = true;

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
	PageItemGroup->SetSelectionRequiredN(true);
	PageItemGroup->SetBroadcastOnDeselected(false);
	PageItemGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UWidgetSettingPanelBase::OnPageItemSelected);

	for(const auto Iter : GetSubWidgets<UWidgetSettingPageBase>())
	{
		SpawnPageItem(Iter);
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

void UWidgetSettingPanelBase::OnPageItemSelected_Implementation(UCommonButtonBase* AssociatedButton, int32 ButtonIndex)
{
	Switcher_Page->SetActiveWidgetIndex(ButtonIndex);
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

UCommonButton* UWidgetSettingPanelBase::SpawnPageItem_Implementation(UWidgetSettingPageBase* InPage)
{
	if(UCommonButton* PageItem = UObjectPoolModuleStatics::SpawnObject<UCommonButton>(InPage, nullptr, PageItemClass))
	{
		PageItem->SetIsSelectable(true);
		PageItem->SetTitle(InPage->GetTitle());
		if(InPage->GetPageItemStyle())
		{
			PageItem->SetStyle(InPage->GetPageItemStyle());
		}
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
	return GetSubWidget<UWidgetSettingPageBase>(GetCurrentPageIndex());
}

void UWidgetSettingPanelBase::SetCurrentPage(int32 InPageIndex)
{
	PageItemGroup->SelectButtonAtIndex(InPageIndex);
}
