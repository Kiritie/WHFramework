// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Sub/SubWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "Widget/Screen/UserWidgetBase.h"

USubWidgetBase::USubWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;

	OwnerWidget = nullptr;
}

void USubWidgetBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void USubWidgetBase::OnDespawn_Implementation(bool bRecovery)
{
	OwnerWidget = nullptr;
}

void USubWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
	
}

void USubWidgetBase::OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams)
{
	OwnerWidget = Cast<IPanelWidgetInterface>(InOwner);

	for(auto Iter : GetPoolWidgets())
	{
		IObjectPoolInterface::Execute_OnSpawn(Iter, nullptr, {});
	}

	K2_OnCreate(InOwner, InParams);

	OnInitialize(InParams);
}

void USubWidgetBase::OnInitialize(const TArray<FParameter>& InParams)
{
	WidgetParams = InParams;
	K2_OnInitialize(InParams);

	OnRefresh();
}

void USubWidgetBase::OnReset(bool bForce)
{
	K2_OnReset(bForce);
}

void USubWidgetBase::OnRefresh()
{
	K2_OnRefresh();
}

void USubWidgetBase::OnDestroy(bool bRecovery)
{
	K2_OnDestroy(bRecovery);
}

void USubWidgetBase::Init(const TArray<FParameter>* InParams)
{
	Init(InParams ? *InParams : TArray<FParameter>());
}

void USubWidgetBase::Init(const TArray<FParameter>& InParams)
{
	OnInitialize(InParams);
}

void USubWidgetBase::Reset(bool bForce)
{
	OnReset(bForce);
}

void USubWidgetBase::Refresh()
{
	OnRefresh();
}

void USubWidgetBase::Destroy(bool bRecovery)
{
	if(OwnerWidget)
	{
		OwnerWidget->DestroySubWidget(this, bRecovery);
	}
}

UUserWidget* USubWidgetBase::GetOwnerWidget(TSubclassOf<UUserWidget> InClass) const
{
	return GetDeterminesOutputObject(Cast<UUserWidget>(OwnerWidget), InClass);
}

TArray<UWidget*> USubWidgetBase::GetPoolWidgets() const
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
