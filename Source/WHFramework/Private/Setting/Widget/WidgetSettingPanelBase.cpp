// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/WidgetSettingPanelBase.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "Input/InputTags.h"
#include "Widget/ScreenWidgetTypes.h"
#include "Setting/SettingPresentationTypes.h"
#include "Setting/SettingEditTypes.h"

#include "CommonActivatableWidgetSwitcher.h"
#include "Components/PanelWidget.h"
#include "Components/HorizontalBoxSlot.h"
#include "Engine/World.h"
#include "Input/InputModule.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"
#include "Setting/SettingModule.h"
#include "Setting/Widget/Page/WidgetSettingPageBase.h"
#include "Widget/WidgetModuleStatics.h"
#include "Widget/Common/CommonButton.h"
#include "Widget/Common/CommonWindow.h"
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

	PageItemGroup = NewObject<UCommonButtonGroup>(this);
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
	SessionStatus = FText::GetEmpty();
	if (!bOwnsEditSession)
	{
		USettingModule* Settings = USettingModule::GetPtr(GetWorld());
		const bool bStarted = Settings && Settings->BeginEdit(GetOwningPlayer());
		if (bStarted)
		{
			EditSessionSource = Settings;
			bOwnsEditSession = true;
		}
		else
		{
			SessionStatus = Settings ? NSLOCTEXT("WH.Setting", "SessionUnavailable", "无法开始编辑：玩家或设置尚未就绪，或设置正在其他界面中编辑。") :
				NSLOCTEXT("WH.Setting", "ServiceUnavailable", "设置服务尚未就绪，请稍后重新打开。");
		}
	}
	BindEditObservers();
	if (GetSubWidgets<UWidgetSettingPageBase>().IsEmpty()) GenerateSettingPages();
	if (PageItemGroup && PageItemGroup->GetButtons().IsEmpty())
	{
		for (UWidgetSettingPageBase* Page : GetSubWidgets<UWidgetSettingPageBase>())
		{
			if (Page) SpawnPageItem(Page);
		}
	}
	RefreshPages();
	Super::OnOpen(InParam, bInstant);
	if (PageItemGroup && !PageItemGroup->GetButtons().IsEmpty()) SetCurrentPage(0);
	RefreshActionButtonState();
}

void UWidgetSettingPanelBase::OnClose(bool bInstant)
{
	ReleaseEditSession();
	Super::OnClose(bInstant);
}

void UWidgetSettingPanelBase::OnDestroy(EObjectDespawnMode InMode)
{
	Window_Content->OnCloseRequested.RemoveDynamic(this, &ThisClass::OnCloseButtonClicked);
	ReleaseEditSession();
	for (UCommonButton* Button : {Btn_Apply, Btn_Reset})
	{
		if (Button)
		{
			Button->OnClicked().RemoveAll(this);
		}
	}
	if (PageItemGroup)
	{
		PageItemGroup->OnSelectedButtonBaseChanged.RemoveDynamic(this, &ThisClass::OnPageItemSelected);
		for (UCommonButtonBase* PageButton : PageItemGroup->GetButtons())
		{
			UObjectPoolModuleStatics::DespawnObject(this, PageButton, InMode);
		}
		PageItemGroup->RemoveAll();
		PageItemGroup = nullptr;
	}
	Super::OnDestroy(InMode);
}

void UWidgetSettingPanelBase::NativeConstruct()
{
	Super::NativeConstruct();
	Window_Content->OnCloseRequested.AddUniqueDynamic(this, &ThisClass::OnCloseButtonClicked);
	if (bOwnsEditSession)
	{
		BindEditObservers();
		RefreshPages();
		RefreshActionButtonState();
	}
}

void UWidgetSettingPanelBase::NativeDestruct()
{
	Window_Content->OnCloseRequested.RemoveDynamic(this, &ThisClass::OnCloseButtonClicked);
	UnbindEditObservers();
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ConfirmationRefreshTimer);
	Super::NativeDestruct();
}

void UWidgetSettingPanelBase::ReleaseEditSession()
{
	if (!bOwnsEditSession && !EditSessionSource.IsValid()) return;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ConfirmationRefreshTimer);
	UnbindEditObservers();
	USettingModule* Settings = EditSessionSource.Get();
	const bool bRelease = bOwnsEditSession;
	bOwnsEditSession = false;
	EditSessionSource.Reset();
	for (UWidgetSettingPageBase* Page : GetSubWidgets<UWidgetSettingPageBase>())
	{
		if (Page) Page->SetSettingSource(nullptr);
	}
	if (bRelease && Settings && Settings->IsEditSessionOwner(GetOwningPlayer())) Settings->EndEdit();
}

USettingModule* UWidgetSettingPanelBase::GetSettingSource() const
{
	const UWorld* World = GetWorld();
	USettingModule* Settings = EditSessionSource.Get();
	return bOwnsEditSession && World && !World->bIsTearingDown && Settings && Settings->GetWorld() == World && Settings->IsEditSessionOwner(GetOwningPlayer()) ? Settings : nullptr;
}

void UWidgetSettingPanelBase::BindEditObservers()
{
	if (USettingModule* Settings = GetSettingSource())
	{
		Settings->OnSettingEditStateChanged.AddUniqueDynamic(this, &ThisClass::OnSettingEditStateChanged);
	}
}

void UWidgetSettingPanelBase::UnbindEditObservers()
{
	if (EditSessionSource.IsValid()) EditSessionSource->OnSettingEditStateChanged.RemoveDynamic(this, &ThisClass::OnSettingEditStateChanged);
}

FReply UWidgetSettingPanelBase::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const UInputModule* Inputs = UInputModule::GetPtr(GetWorld());
	const int32 PlayerIndex = Inputs ? Inputs->FindPlayerRuntimeIndex(GetOwningPlayer()) : INDEX_NONE;
	if (Inputs && PlayerIndex != INDEX_NONE && Inputs->IsPlayerMappedKeyByTag(GameplayTags::Input_SystemOperation, InKeyEvent.GetKey(), PlayerIndex))
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
	USettingModule* Settings = GetSettingSource();
	if (!Settings || !Settings->CanApply()) return;
	const bool bApplied = Settings->ApplyEditSession();
	SessionStatus = bApplied ? FText::GetEmpty() : NSLOCTEXT("WH.Setting", "ApplyFailed", "设置未能应用，请检查输入或稍后重试。");
	if (bApplied && !Settings->HasPendingConfirmation())
	{
		const UInputModule* Input = UInputModule::GetPtr(GetWorld());
		if (!Settings->WasLastSettingsSaveSuccessful())
			SessionStatus = NSLOCTEXT("WH.Setting", "SaveFailed", "设置已在本次会话生效，但保存失败。");
		else if (!Input || !Input->HasPersistentInputProfile(GetOwningPlayer()))
			SessionStatus = NSLOCTEXT("WH.Setting", "GuestInput", "设置已应用；当前玩家未绑定本地档案，输入和相机设置仅在本次会话保留。");
	}
	if (bApplied && Settings->HasPendingConfirmation()) K2_OnConfirmationRequested(Settings->GetConfirmationTimeout());
	RefreshPages();
	RefreshActionButtonState();
}

void UWidgetSettingPanelBase::OnResetButtonClicked()
{
	USettingModule* Settings = GetSettingSource();
	if (!Settings || Settings->HasPendingConfirmation()) return;
	SessionStatus = FText::GetEmpty();
	Settings->ResetAllToDefault();
	RefreshPages();
	RefreshActionButtonState();
}

void UWidgetSettingPanelBase::RefreshPages()
{
	for(UWidgetSettingPageBase* Page : GetSubWidgets<UWidgetSettingPageBase>())
	{
		if(Page)
		{
			Page->SetSettingSource(GetSettingSource());
		}
	}
}

UCommonButton* UWidgetSettingPanelBase::SpawnPageItem_Implementation(UWidgetSettingPageBase* InPage)
{
	FWidgetSpawnParameter Spawn(InPage);
	Spawn.OwningPlayer = GetOwningPlayer();
	if(UCommonButton* PageItem = UObjectPoolModuleStatics::SpawnObject<UCommonButton>(
		this, Spawn,
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
	USettingModule* Settings = GetSettingSource();
	if (!Settings) return;
	for(const FSettingPageDefinition& PageDefinition : Settings->GetSettingPages())
	{
		if(UWidgetSettingPageBase* Page = CreateSubWidget<UWidgetSettingPageBase>(FParameter(), SettingPageClass))
		{
			Page->SetSettingSource(Settings);
			Page->SetPageDefinition(PageDefinition);
			Switcher_Page->AddChild(Page);
		}
	}
}

void UWidgetSettingPanelBase::OnSettingEditStateChanged(const FSettingEditState& InState)
{
	USettingModule* Settings = GetSettingSource();
	const bool bPending = Settings && Settings->HasPendingConfirmation();
	if (Btn_Apply) Btn_Apply->SetIsEnabledN(Settings && InState.bCanApply && !bPending);
	if (Btn_Reset) Btn_Reset->SetIsEnabledN(Settings && InState.bCanReset && !bPending);
	if (Switcher_Page) Switcher_Page->SetIsEnabled(Settings && !bPending);
	if (PageItemContainer) PageItemContainer->SetIsEnabled(Settings && !bPending);
	FText Message = SessionStatus;
	if (!Settings && Message.IsEmpty()) Message = NSLOCTEXT("WH.Setting", "ServiceUnavailable", "设置服务尚未就绪，请稍后重新打开。");
	Txt_Status->SetText(Message);
	Txt_Status->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	RefreshConfirmation();
}

void UWidgetSettingPanelBase::RefreshConfirmation()
{
	if (!GetWorld()) return;
	const USettingModule* Settings = GetSettingSource();
	const bool bPending = Settings && Settings->HasPendingConfirmation();
	if (bPending)
	{
		if (!GetWorld()->GetTimerManager().IsTimerActive(ConfirmationRefreshTimer))
		{
			GetWorld()->GetTimerManager().SetTimer(ConfirmationRefreshTimer, this, &ThisClass::RefreshConfirmation, .25f, true);
		}
	}
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(ConfirmationRefreshTimer);
	}
}

void UWidgetSettingPanelBase::OnCloseButtonClicked()
{
	Close();
}

void UWidgetSettingPanelBase::RefreshActionButtonState()
{
	const USettingModule* Settings = GetSettingSource();
	OnSettingEditStateChanged(Settings ? Settings->GetEditState() : FSettingEditState());
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
	USettingModule* Settings = GetSettingSource();
	if (!Settings) return false;
	const bool bResult = Settings->ConfirmPendingSettings();
	if (bResult)
	{
		const UInputModule* Input = UInputModule::GetPtr(GetWorld());
		if (!Settings->WasLastSettingsSaveSuccessful()) SessionStatus = NSLOCTEXT("WH.Setting", "ConfirmSaveFailed", "设置已在本次会话生效，但保存失败。");
		else if (!Input || !Input->HasPersistentInputProfile(GetOwningPlayer())) SessionStatus = NSLOCTEXT("WH.Setting", "ConfirmGuestInput", "设置已应用；当前玩家未绑定本地档案，输入和相机设置仅在本次会话保留。");
		RefreshPages();
		RefreshActionButtonState();
	}
	return bResult;
}

void UWidgetSettingPanelBase::RejectPendingSettings()
{
	USettingModule* Settings = GetSettingSource();
	if (!Settings) return;
	if (!Settings->RejectPendingSettings()) SessionStatus = NSLOCTEXT("WH.Setting", "RejectFailed", "未能恢复原设置，请稍后重试。");
	RefreshPages();
}
