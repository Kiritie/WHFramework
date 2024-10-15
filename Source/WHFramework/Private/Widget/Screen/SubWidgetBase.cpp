// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Screen/SubWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "Widget/Screen/UserWidgetBase.h"

USubWidgetBase::USubWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	OwnerWidget = nullptr;
}

void USubWidgetBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void USubWidgetBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
	
	OwnerWidget = nullptr;
}

void USubWidgetBase::OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams)
{
	OwnerWidget = InOwner;

	for(auto Iter : GetAllPoolWidgets())
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

UUserWidgetBase* USubWidgetBase::GetOwnerWidget(TSubclassOf<UUserWidgetBase> InClass) const
{
	return GetDeterminesOutputObject(OwnerWidget, InClass);
}

TArray<UWidget*> USubWidgetBase::GetAllPoolWidgets() const
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
