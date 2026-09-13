// Fill out your copyright notice in the Description page of Project Settings.

#include "Widget/Pool/PoolWidgetBase.h"

#include "Common/CommonModuleTypes.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "Widget/WidgetModule.h"

UPoolWidgetBase::UPoolWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;

	WidgetParam = FParameter();
}

void UPoolWidgetBase::OnSpawn_Implementation(const FParameter& InParam)
{
	if(UWidgetModule::IsValid()) UWidgetModule::Get().RegisterTickableWidget(this);

	const FWidgetSpawnParameter* Parameter = InParam.GetPtr<FWidgetSpawnParameter>();
	OwnerWidget = Parameter ? Cast<UUserWidget>(Parameter->OwnerObject) : nullptr;
	WidgetParam = InParam;

	Refresh();
}

void UPoolWidgetBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	if(UWidgetModule::IsValid()) UWidgetModule::Get().UnregisterTickableWidget(this);

	OwnerWidget = nullptr;
	WidgetParam.Reset();
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

void UPoolWidgetBase::Destroy(EObjectDespawnMode InMode)
{
	UObjectPoolModuleStatics::DespawnObject(this, InMode);
}

UUserWidget* UPoolWidgetBase::GetOwnerWidget(TSubclassOf<UUserWidget> InClass) const
{
	return GetDeterminesOutputObject(OwnerWidget, InClass);
}
