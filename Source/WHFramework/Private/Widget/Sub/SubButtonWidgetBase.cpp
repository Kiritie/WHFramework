// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Sub/SubButtonWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Widget/Screen/UserWidgetBase.h"

USubButtonWidgetBase::USubButtonWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;

	OwnerWidget = nullptr;
}

void USubButtonWidgetBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void USubButtonWidgetBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
}

void USubButtonWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
	
}

void USubButtonWidgetBase::OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams)
{
	OwnerWidget = Cast<IPanelWidgetInterface>(InOwner);

	for(auto Iter : GetPoolWidgets())
	{
		IObjectPoolInterface::Execute_OnSpawn(Iter, nullptr, {});
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
	RemoveFromParent();

	K2_OnDestroy(bRecovery);

	UObjectPoolModuleStatics::DespawnObject(this, bRecovery);

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
