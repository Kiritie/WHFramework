// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/User/UserWidgetBase.h"

#include "WidgetModuleBPLibrary.h"

UUserWidgetBase::UUserWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetType = EWidgetType::None;
	InputMode = EInputMode::None;
	OwnerActor = nullptr;
}

void UUserWidgetBase::OnInitialize_Implementation(AActor* InOwner)
{
	OwnerActor = InOwner;
	if(!IsInViewport())
	{
		AddToViewport();
	}
}

void UUserWidgetBase::OnOpen_Implementation(bool bInstant)
{
	if(bInstant)
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	OnRefresh();
}

void UUserWidgetBase::OnClose_Implementation(bool bInstant)
{
	if(bInstant)
	{
		SetVisibility(ESlateVisibility::Hidden);
	}
}

void UUserWidgetBase::OnToggle_Implementation(bool bInstant)
{
	if(GetVisibility() == ESlateVisibility::Hidden)
	{
		OpenSelf(bInstant);
	}
	else
	{
		CloseSelf(bInstant);
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

void UUserWidgetBase::OpenSelf_Implementation(bool bInstant)
{
	UWidgetModuleBPLibrary::OpenUserWidget<UUserWidgetBase>(this, bInstant, StaticClass());
}

void UUserWidgetBase::CloseSelf_Implementation(bool bInstant)
{
	UWidgetModuleBPLibrary::CloseUserWidget<UUserWidgetBase>(this, bInstant, StaticClass());
}
