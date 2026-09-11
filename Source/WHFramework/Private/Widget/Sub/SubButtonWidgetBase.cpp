// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Sub/SubButtonWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Widget/Screen/UserWidgetBase.h"
#include "Widget/WidgetModule.h"

USubButtonWidgetBase::USubButtonWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;

	OwnerWidget = nullptr;
}

void USubButtonWidgetBase::OnSpawn_Implementation(
	const FParameter& InParameter)
{
	Super::OnSpawn_Implementation(InParameter);
}

void USubButtonWidgetBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);
}

void USubButtonWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
	
}

void USubButtonWidgetBase::OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams)
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

void USubButtonWidgetBase::OnInitialize(const TArray<FParameter>& InParams)
{
	WidgetParams = InParams;
	K2_OnInitialize(InParams);

	OnRefresh();
}

void USubButtonWidgetBase::OnReset(bool bForce)
{
	K2_OnReset(bForce);
}

void USubButtonWidgetBase::OnRefresh()
{
	K2_OnRefresh();
}

void USubButtonWidgetBase::OnDestroy(bool bRecovery)
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

void USubButtonWidgetBase::Init(const TArray<FParameter>* InParams)
{
	Init(InParams ? *InParams : TArray<FParameter>());
}

void USubButtonWidgetBase::Init(const TArray<FParameter>& InParams)
{
	OnInitialize(InParams);
}

void USubButtonWidgetBase::Reset(bool bForce)
{
	OnReset(bForce);
}

void USubButtonWidgetBase::Refresh()
{
	OnRefresh();
}

void USubButtonWidgetBase::Destroy(bool bRecovery)
{
	if(OwnerWidget)
	{
		OwnerWidget->DestroySubWidget(this, bRecovery);
	}
}

UUserWidget* USubButtonWidgetBase::GetOwnerWidget(TSubclassOf<UUserWidget> InClass) const
{
	return GetDeterminesOutputObject(Cast<UUserWidget>(OwnerWidget), InClass);
}

TArray<UWidget*> USubButtonWidgetBase::GetPoolWidgets() const
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
