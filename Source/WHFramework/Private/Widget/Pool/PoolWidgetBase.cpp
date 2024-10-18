// Fill out your copyright notice in the Description page of Project Settings.

#include "Widget/Pool/PoolWidgetBase.h"

#include "Common/CommonTypes.h"

UPoolWidgetBase::UPoolWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	OwnerWidget = nullptr;
}

void UPoolWidgetBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	OwnerWidget = Cast<UUserWidget>(InOwner);
	WidgetParams = InParams;
}

void UPoolWidgetBase::OnDespawn_Implementation(bool bRecovery)
{
	OwnerWidget = nullptr;
	WidgetParams.Empty();
}

UUserWidget* UPoolWidgetBase::GetOwnerWidget(TSubclassOf<UUserWidget> InClass) const
{
	return GetDeterminesOutputObject(OwnerWidget, InClass);
}
