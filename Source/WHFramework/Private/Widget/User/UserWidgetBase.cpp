// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/User/UserWidgetBase.h"

UUserWidgetBase::UUserWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetType = EWHWidgetType::None;
	InputMode = EWHInputMode::None;
}

void UUserWidgetBase::OnInitialize_Implementation()
{
	
}

void UUserWidgetBase::OnAttach_Implementation()
{
}

void UUserWidgetBase::OnRefresh_Implementation(float DeltaSeconds)
{
}

void UUserWidgetBase::OnOpen_Implementation()
{
}

void UUserWidgetBase::OnClose_Implementation()
{
}

void UUserWidgetBase::OnDetach_Implementation()
{
}

void UUserWidgetBase::OnDestroy_Implementation()
{
}

void UUserWidgetBase::OpenSelf_Implementation()
{
}

void UUserWidgetBase::CloseSelf_Implementation()
{
}
