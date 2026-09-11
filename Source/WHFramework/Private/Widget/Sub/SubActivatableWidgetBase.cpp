// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Sub/SubActivatableWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Widget/Screen/UserWidgetBase.h"
#include "Widget/WidgetModule.h"

USubActivatableWidgetBase::USubActivatableWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;

	OwnerWidget = nullptr;
}

void USubActivatableWidgetBase::OnSpawn_Implementation(
	const FParameter& InParameter)
{
	
}

void USubActivatableWidgetBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	
}

void USubActivatableWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
	
}

void USubActivatableWidgetBase::OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams)
{
	if(UWidgetModule::IsValid()) UWidgetModule::Get().RegisterTickableWidget(this);

	OwnerWidget = Cast<IPanelWidgetInterface>(InOwner);

	for(auto Iter : GetPoolWidgets())
	{
		IObjectPoolInterface::Execute_OnSpawn(Iter, FParameter(FWidgetSpawnParameter(this)));
	}

	K2_OnCreate(InOwner, InParams);

	OnInitialize(InParams);
}

void USubActivatableWidgetBase::OnInitialize(const TArray<FParameter>& InParams)
{
	WidgetParams = InParams;
	K2_OnInitialize(InParams);

	OnRefresh();
}

void USubActivatableWidgetBase::OnReset(bool bForce)
{
	K2_OnReset(bForce);
}

void USubActivatableWidgetBase::OnRefresh()
{
	K2_OnRefresh();
}

void USubActivatableWidgetBase::OnDestroy(bool bRecovery)
{
	if(UWidgetModule::IsValid()) UWidgetModule::Get().UnregisterTickableWidget(this);

	RemoveFromParent();

	K2_OnDestroy(bRecovery);

	UObjectPoolModuleStatics::DespawnObject(
		this,
		bRecovery ? EObjectDespawnMode::Recovery : EObjectDespawnMode::Destroy);

	OwnerWidget = nullptr;
	WidgetParams.Empty();
}

void USubActivatableWidgetBase::NativeOnActivated()
{
	Super::NativeOnActivated();
}

void USubActivatableWidgetBase::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
}

void USubActivatableWidgetBase::Init(const TArray<FParameter>* InParams)
{
	Init(InParams ? *InParams : TArray<FParameter>());
}

void USubActivatableWidgetBase::Init(const TArray<FParameter>& InParams)
{
	OnInitialize(InParams);
}

void USubActivatableWidgetBase::Reset(bool bForce)
{
	OnReset(bForce);
}

void USubActivatableWidgetBase::Refresh()
{
	OnRefresh();
}

void USubActivatableWidgetBase::Destroy(bool bRecovery)
{
	if(OwnerWidget)
	{
		OwnerWidget->DestroySubWidget(this, bRecovery);
	}
}

UUserWidget* USubActivatableWidgetBase::GetOwnerWidget(TSubclassOf<UUserWidget> InClass) const
{
	return GetDeterminesOutputObject(Cast<UUserWidget>(OwnerWidget), InClass);
}

TArray<UWidget*> USubActivatableWidgetBase::GetPoolWidgets() const
{
	TArray<UWidget*> PoolWidgets;
	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);
	for(auto Iter : Widgets)
	{
		if(Iter->Implements<UObjectPoolInterface>())
		{
			PoolWidgets.Add(Iter);
		}
	}
	return PoolWidgets;
}
