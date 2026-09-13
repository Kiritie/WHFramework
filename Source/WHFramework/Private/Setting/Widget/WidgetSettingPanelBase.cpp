// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/WidgetSettingPanelBase.h"

#include "CommonActivatableWidgetSwitcher.h"
#include "Components/PanelWidget.h"
#include "Setting/SettingModule.h"
#include "Setting/Widget/Page/WidgetSettingPageBase.h"
#include "Widget/WidgetModuleStatics.h"
#include "Widget/Common/CommonButton.h"
#include "Widget/Common/CommonButtonGroup.h"

UWidgetSettingPanelBase::UWidgetSettingPanelBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

	bWidgetAutoFocus = true;

	SetIsFocusable(true);
	
	PageItemGroup = nullptr;
	PageItemClass = nullptr;
	SettingPageClass = UWidgetSettingPageBase::StaticClass();
}

void UWidgetSettingPanelBase::OnCreate(const FParameter& InParam)
{
	Super::OnCreate(InParam);

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
}

void UWidgetSettingPanelBase::OnInitialize(const FParameter& InParam)
{
	Super::OnInitialize(InParam);
}

void UWidgetSettingPanelBase::OnOpen(const FParameter& InParam, bool bInstant)
{
	USettingModule::Get().BeginEdit();
	if(GetSubWidgets<UWidgetSettingPageBase>().IsEmpty())
	{
		GenerateSettingPages();
	}
	if(PageItemGroup && PageItemGroup->GetButtons().IsEmpty())
	{
		for(UWidgetSettingPageBase* Page : GetSubWidgets<UWidgetSettingPageBase>())
		{
			if(Page)
			{
				SpawnPageItem(Page);
			}
		}
	}

	Super::OnOpen(InParam, bInstant);

	for(UWidgetSettingPageBase* Page : GetSubWidgets<UWidgetSettingPageBase>())
	{
		if(Page)
		{
			Page->RefreshEntries();
		}
	}
	
	if(PageItemGroup && !PageItemGroup->GetButtons().IsEmpty())
	{
		SetCurrentPage(0);
	}
}

void UWidgetSettingPanelBase::OnClose(bool bInstant)
{
	USettingModule::Get().CancelEditSession();
	USettingModule::Get().EndEdit();
	Super::OnClose(bInstant);
}

void UWidgetSettingPanelBase::OnPageItemSelected_Implementation(UCommonButtonBase* AssociatedButton, int32 ButtonIndex)
{
	Switcher_Page->SetActiveWidgetIndex(ButtonIndex);
}

void UWidgetSettingPanelBase::OnApplyButtonClicked()
{
	if(USettingModule::Get().ApplyEditSession())
	{
		if(USettingModule::Get().HasPendingConfirmation())
		{
			K2_OnConfirmationRequested(USettingModule::Get().GetConfirmationTimeout());
		}
		for(UWidgetSettingPageBase* Page : GetSubWidgets<UWidgetSettingPageBase>())
		{
			if(Page)
			{
				Page->RefreshEntries();
			}
		}
	}
}

void UWidgetSettingPanelBase::OnResetButtonClicked()
{
	USettingModule::Get().ResetAllToDefault();
	for(UWidgetSettingPageBase* Page : GetSubWidgets<UWidgetSettingPageBase>())
	{
		if(Page)
		{
			Page->RefreshEntries();
		}
	}
}

UCommonButton* UWidgetSettingPanelBase::SpawnPageItem_Implementation(UWidgetSettingPageBase* InPage)
{
	if(UCommonButton* PageItem = UObjectPoolModuleStatics::SpawnObject<UCommonButton>(
		FWidgetSpawnParameter(InPage),
		PageItemClass))
	{
		PageItem->SetIsSelectable(true);
		PageItem->SetTitle(InPage->GetTitle());
		PageItem->SetStyleTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Style.Button.Setting.PageTab")), false));
		PageItemGroup->AddWidget(PageItem);
		if(PageItemContainer)
		{
			PageItemContainer->AddChild(PageItem);
		}
		return PageItem;
	}
	return nullptr;
}

void UWidgetSettingPanelBase::GenerateSettingPages()
{
	if(!SettingPageClass || !Switcher_Page)
	{
		return;
	}
	for(const FSettingPageDefinition& PageDefinition : USettingModule::Get().GetSettingPages())
	{
		if(UWidgetSettingPageBase* Page = CreateSubWidget<UWidgetSettingPageBase>(FParameter(), SettingPageClass))
		{
			Page->SetPageDefinition(PageDefinition);
			Switcher_Page->AddChild(Page);
		}
	}
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

bool UWidgetSettingPanelBase::ConfirmPendingSettings()
{
	const bool bResult = USettingModule::Get().ConfirmPendingSettings();
	if(bResult)
	{
		for(UWidgetSettingPageBase* Page : GetSubWidgets<UWidgetSettingPageBase>())
		{
			if(Page)
			{
				Page->RefreshEntries();
			}
		}
	}
	return bResult;
}

void UWidgetSettingPanelBase::RejectPendingSettings()
{
	USettingModule::Get().RejectPendingSettings();
	for(UWidgetSettingPageBase* Page : GetSubWidgets<UWidgetSettingPageBase>())
	{
		if(Page)
		{
			Page->RefreshEntries();
		}
	}
}
