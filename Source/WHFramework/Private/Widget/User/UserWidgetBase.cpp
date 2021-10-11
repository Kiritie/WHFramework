// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/User/UserWidgetBase.h"

#include "WidgetModuleBPLibrary.h"

UUserWidgetBase::UUserWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = NAME_None;
	WidgetType = EWidgetType::None;
	InputMode = EInputMode::None;
	OwnerActor = nullptr;
}

void UUserWidgetBase::OnCreate_Implementation()
{
	
}

void UUserWidgetBase::OnInitialize_Implementation(AActor* InOwner)
{
	OwnerActor = InOwner;
}

void UUserWidgetBase::OnOpen_Implementation(bool bInstant)
{
	if(!IsInViewport())
	{
		AddToViewport();
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Refresh();
	
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		WidgetModule->UpdateInputMode();
	}
}

void UUserWidgetBase::OnClose_Implementation(bool bInstant)
{
	if(WidgetType == EWidgetType::Temporary)
	{
		if(IsInViewport())
		{
			RemoveFromViewport();
		}
	}
	SetVisibility(ESlateVisibility::Hidden);
	
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		WidgetModule->UpdateInputMode();
	}
}

void UUserWidgetBase::OnRefresh_Implementation()
{
}

void UUserWidgetBase::OnDestroy_Implementation()
{
	if(IsInViewport())
	{
		RemoveFromViewport();
	}
}

void UUserWidgetBase::Open_Implementation(bool bInstant)
{
	UWidgetModuleBPLibrary::OpenUserWidget<UUserWidgetBase>(bInstant, GetClass());
}

void UUserWidgetBase::Close_Implementation(bool bInstant)
{
	UWidgetModuleBPLibrary::CloseUserWidget<UUserWidgetBase>(bInstant, GetClass());
}

void UUserWidgetBase::Toggle_Implementation(bool bInstant)
{
	if(!IsOpened())
	{
		Open(bInstant);
	}
	else
	{
		Close(bInstant);
	}
}

void UUserWidgetBase::Refresh_Implementation()
{
	if(IsOpened())
	{
		OnRefresh();
	}
}

bool UUserWidgetBase::IsOpened() const
{
	return IsInViewport() && GetVisibility() != ESlateVisibility::Hidden && GetVisibility() != ESlateVisibility::Collapsed;
}
