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

void UUserWidgetBase::OnRefresh_Implementation()
{
}

void UUserWidgetBase::OnOpen_Implementation()
{
	OnRefresh();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UUserWidgetBase::OnClose_Implementation()
{
	SetVisibility(ESlateVisibility::Hidden);
}

void UUserWidgetBase::OnToggle_Implementation()
{
	if(GetVisibility() == ESlateVisibility::Hidden)
	{
		OpenSelf();
	}
	else
	{
		CloseSelf();
	}
}

void UUserWidgetBase::OnDestroy_Implementation()
{
	if(IsInViewport())
	{
		RemoveFromViewport();
	}
}

void UUserWidgetBase::OpenSelf_Implementation()
{
	UWidgetModuleBPLibrary::OpenUserWidget<UUserWidgetBase>(this, StaticClass());
}

void UUserWidgetBase::CloseSelf_Implementation()
{
	UWidgetModuleBPLibrary::CloseUserWidget<UUserWidgetBase>(this, StaticClass());
}
