// Fill out your copyright notice in the Description page of Project Settings.

#include "Widget/Pool/PoolWidgetBase.h"

#include "Common/CommonTypes.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"

UPoolWidgetBase::UPoolWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;

	OwnerWidget = nullptr;
}

void UPoolWidgetBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	OwnerWidget = Cast<UUserWidget>(InOwner);
	WidgetParams = InParams;

	Refresh();
}

void UPoolWidgetBase::OnDespawn_Implementation(bool bRecovery)
{
	OwnerWidget = nullptr;
	WidgetParams.Empty();

	RemoveFromParent();
}

void UPoolWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
	
}

void UPoolWidgetBase::OnRefresh()
{
	K2_OnRefresh();
}

void UPoolWidgetBase::Refresh()
{
	OnRefresh();
}

void UPoolWidgetBase::Destroy(bool bRecovery)
{
	UObjectPoolModuleStatics::DespawnObject(this, bRecovery);
}

UUserWidget* UPoolWidgetBase::GetOwnerWidget(TSubclassOf<UUserWidget> InClass) const
{
	return GetDeterminesOutputObject(OwnerWidget, InClass);
}
