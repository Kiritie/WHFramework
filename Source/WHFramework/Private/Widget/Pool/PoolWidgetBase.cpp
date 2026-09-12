// Fill out your copyright notice in the Description page of Project Settings.

#include "Widget/Pool/PoolWidgetBase.h"

#include "Common/CommonModuleTypes.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "Widget/WidgetModule.h"

UPoolWidgetBase::UPoolWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;

	OwnerWidget = nullptr;
}

void UPoolWidgetBase::OnSpawn_Implementation(const FParameter& InParam)
{
	if(UWidgetModule::IsValid()) UWidgetModule::Get().RegisterTickableWidget(this);

	const FWidgetSpawnParameter* Parameter = InParam.GetPtr<FWidgetSpawnParameter>();
	OwnerWidget = Parameter ? Cast<UUserWidget>(Parameter->OwningObject) : nullptr;

	Refresh();
}

void UPoolWidgetBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	if(UWidgetModule::IsValid()) UWidgetModule::Get().UnregisterTickableWidget(this);

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
	UObjectPoolModuleStatics::DespawnObject(
		this,
		bRecovery ? EObjectDespawnMode::Recovery : EObjectDespawnMode::Destroy);
}

UUserWidget* UPoolWidgetBase::GetOwnerWidget(TSubclassOf<UUserWidget> InClass) const
{
	return GetDeterminesOutputObject(OwnerWidget, InClass);
}
