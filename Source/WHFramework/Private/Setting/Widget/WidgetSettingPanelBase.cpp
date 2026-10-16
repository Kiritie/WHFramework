// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/WidgetSettingPanelBase.h"

#include "CommonActivatableWidgetSwitcher.h"
#include "Components/PanelWidget.h"
#include "Components/HorizontalBoxSlot.h"
#include "Engine/World.h"
#include "Input/InputModule.h"
#include "Input/InputModuleStatics.h"
#include "TimerManager.h"
#include "Setting/SettingModule.h"
#include "Setting/Widget/Page/WidgetSettingPageBase.h"
#include "Widget/WidgetModuleStatics.h"
#include "Widget/Common/CommonButton.h"
#include "Widget/Common/CommonButtonGroup.h"
#include "Widget/Common/CommonTextBlockN.h"

UWidgetSettingPanelBase::UWidgetSettingPanelBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

	bWidgetAutoFocus = true;
	WidgetInputMode = EWidgetInputMode::UIOnly;
	bWidgetActivatable = true;

	SetIsFocusable(true);
	
	PageItemGroup = nullptr;
	PageItemClass = UCommonButton::StaticClass();
	SettingPageClass = nullptr;
}

void UWidgetSettingPanelBase::OnCreate(const FParameter& InParam)
{
	const auto BindButton = [this](UCommonButton* Button, void (UWidgetSettingPanelBase::*Callback)())
	{
		if (Button)
		{
			Button->OnClicked().RemoveAll(this);
			Button->OnClicked().AddUObject(this, Callback);
		}
	};
	BindButton(Btn_Apply, &ThisClass::OnApplyButtonClicked);
	BindButton(Btn_Reset, &ThisClass::OnResetButtonClicked);
	BindButton(Btn_Close, &ThisClass::OnCloseButtonClicked);
	BindButton(Btn_Confirm, &ThisClass::OnConfirmButtonClicked);
	BindButton(Btn_Revert, &ThisClass::OnRevertButtonClicked);
	USettingModule::Get().OnSettingEditStateChanged.AddUniqueDynamic(this, &ThisClass::OnSettingEditStateChanged);

	PageItemGroup = UObjectPoolModuleStatics::SpawnObject<UCommonButtonGroup>();
	PageItemGroup->SetSelectionRequiredN(true);
	PageItemGroup->SetBroadcastOnDeselected(false);
	PageItemGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UWidgetSettingPanelBase::OnPageItemSelected);
	Super::OnCreate(InParam);
	RefreshActionButtonState();
}

void UWidgetSettingPanelBase::OnInitialize(const FParameter& InParam)
{
	Super::OnInitialize(InParam);
}

void UWidgetSettingPanelBase::OnOpen(const FParameter& InParam, bool bInstant)
{
	USettingModule::Get().BeginEdit();
	RefreshActionButtonState();
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

	RefreshPages();
	
	if(PageItemGroup && !PageItemGroup->GetButtons().IsEmpty())
	{
		SetCurrentPage(0);
	}
}

void UWidgetSettingPanelBase::OnClose(bool bInstant)
{
	USettingModule::Get().CancelEditSession();
	USettingModule::Get().EndEdit();
	GetWorld()->GetTimerManager().ClearTimer(ConfirmationRefreshTimer);
	Super::OnClose(bInstant);
}

void UWidgetSettingPanelBase::OnDestroy(EObjectDespawnMode InMode)
{
	USettingModule::Get().OnSettingEditStateChanged.RemoveDynamic(this, &ThisClass::OnSettingEditStateChanged);
	GetWorld()->GetTimerManager().ClearTimer(ConfirmationRefreshTimer);
	for (UCommonButton* Button : {Btn_Apply, Btn_Reset, Btn_Close.Get(), Btn_Confirm.Get(), Btn_Revert.Get()})
	{
		if (Button)
		{
			Button->OnClicked().RemoveAll(this);
		}
	}
	if (PageItemGroup)
	{
		PageItemGroup->OnSelectedButtonBaseChanged.RemoveDynamic(this, &ThisClass::OnPageItemSelected);
		UObjectPoolModuleStatics::DespawnObject(PageItemGroup, InMode);
		PageItemGroup = nullptr;
	}
	Super::OnDestroy(InMode);
}

FReply UWidgetSettingPanelBase::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (GetWorld() && GetWorld()->IsGameWorld() && UInputModule::IsValid() &&
		UInputModuleStatics::IsPlayerMappedKeyByTag(GameplayTags::Input_SystemOperation, InKeyEvent.GetKey()))
	{
		Close();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
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
		RefreshPages();
	}
}

void UWidgetSettingPanelBase::OnResetButtonClicked()
{
	USettingModule::Get().ResetAllToDefault();
	RefreshPages();
}

void UWidgetSettingPanelBase::RefreshPages()
{
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
		PageItem->SetInheritedStyleTag(PageItemStyleTag);
		PageItemGroup->AddWidget(PageItem);
		if(PageItemContainer)
		{
			if (UHorizontalBoxSlot* PageSlot = Cast<UHorizontalBoxSlot>(PageItemContainer->AddChild(PageItem)))
			{
				PageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
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

void UWidgetSettingPanelBase::OnSettingEditStateChanged(const FSettingEditState& InState)
{
	if(Btn_Apply)
	{
		Btn_Apply->SetIsEnabled(InState.bCanApply);
	}
	if(Btn_Reset)
	{
		Btn_Reset->SetIsEnabled(InState.bCanReset);
	}
	const bool bPending = USettingModule::Get().HasPendingConfirmation();
	if (Switcher_Page)
	{
		Switcher_Page->SetIsEnabled(!bPending);
	}
	if (PageItemContainer)
	{
		PageItemContainer->SetIsEnabled(!bPending);
	}
	RefreshConfirmation();
}

void UWidgetSettingPanelBase::RefreshConfirmation()
{
	const bool bPending = USettingModule::Get().HasPendingConfirmation();
	const bool bWasShown = ConfirmationContent && ConfirmationContent->GetVisibility() == ESlateVisibility::Visible;
	if (ConfirmationContent)
	{
		ConfirmationContent->SetVisibility(bPending ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (bPending && ConfirmationContent)
	{
		if (Txt_Confirmation)
		{
			Txt_Confirmation->SetText(FText::Format(NSLOCTEXT("WH.Setting", "ConfirmSettings", "是否保留这些设置？{0} 秒后自动还原。"),
				FText::AsNumber(FMath::CeilToInt(USettingModule::Get().GetConfirmationTimeRemaining()))));
		}
		if (!GetWorld()->GetTimerManager().IsTimerActive(ConfirmationRefreshTimer))
		{
			GetWorld()->GetTimerManager().SetTimer(ConfirmationRefreshTimer, this, &ThisClass::RefreshConfirmation, .25f, true);
		}
		if (!bWasShown && Btn_Confirm)
		{
			Btn_Confirm->SetFocus();
		}
	}
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(ConfirmationRefreshTimer);
		if (bWasShown && PageItemGroup && IsWidgetOpened())
		{
			if (UCommonButtonBase* PageButton = PageItemGroup->GetButtonBaseAtIndex(GetCurrentPageIndex()))
			{
				PageButton->SetFocus();
			}
		}
	}
}

void UWidgetSettingPanelBase::OnCloseButtonClicked()
{
	Close();
}

void UWidgetSettingPanelBase::OnConfirmButtonClicked()
{
	ConfirmPendingSettings();
}

void UWidgetSettingPanelBase::OnRevertButtonClicked()
{
	RejectPendingSettings();
}

void UWidgetSettingPanelBase::RefreshActionButtonState()
{
	OnSettingEditStateChanged(USettingModule::Get().GetEditState());
}

int32 UWidgetSettingPanelBase::GetCurrentPageIndex() const
{
	return PageItemGroup ? PageItemGroup->GetSelectedButtonIndex() : INDEX_NONE;
}

UWidgetSettingPageBase* UWidgetSettingPanelBase::GetCurrentPage() const
{
	return GetSubWidget<UWidgetSettingPageBase>(GetCurrentPageIndex());
}

void UWidgetSettingPanelBase::SetCurrentPage(int32 InPageIndex)
{
	if (PageItemGroup)
	{
		PageItemGroup->SelectButtonAtIndex(InPageIndex);
	}
}

bool UWidgetSettingPanelBase::ConfirmPendingSettings()
{
	const bool bResult = USettingModule::Get().ConfirmPendingSettings();
	if(bResult)
	{
		RefreshPages();
	}
	return bResult;
}

void UWidgetSettingPanelBase::RejectPendingSettings()
{
	USettingModule::Get().RejectPendingSettings();
	RefreshPages();
}
