// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/WidgetSettingPanelBase.h"

#include "Components/TextBlock.h"
#include "Setting/Widget/Page/WidgetSettingPageBase.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetSettingPanelBase::UWidgetSettingPanelBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = FName("SettingPanel");

	ChildNames.Add(FName("AudioSettingPage"));
	ChildNames.Add(FName("VideoSettingPage"));
	ChildNames.Add(FName("CameraSettingPage"));
	ChildNames.Add(FName("InputSettingPage"));

	WidgetType = EWidgetType::Temporary;
	WidgetInputMode = EInputMode::UIOnly;

	WidgetZOrder = 10;

	SetIsFocusable(true);
}

void UWidgetSettingPanelBase::OnInitialize(UObject* InOwner)
{
	Super::OnInitialize(InOwner);
}

void UWidgetSettingPanelBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);
}

void UWidgetSettingPanelBase::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);
}

void UWidgetSettingPanelBase::ChangePage(int32 InPageIndex)
{
	PageIndex = InPageIndex;
	if(const auto Page = GetChild<UWidgetSettingPageBase>(PageIndex))
	{
		Page->Open();
		Txt_Title->SetText(Page->Title);
	}
}
