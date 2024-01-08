// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Screen/UMG/SubWidgetBase.h"

#include "Widget/Screen/UMG/UserWidgetBase.h"

USubWidgetBase::USubWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	OwnerWidget = nullptr;
}

void USubWidgetBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{

}

void USubWidgetBase::OnDespawn_Implementation(bool bRecovery)
{
	OwnerWidget = nullptr;
	RemoveFromParent();
}

void USubWidgetBase::OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams)
{
	OwnerWidget = InOwner;
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

void USubWidgetBase::OnDestroy()
{
	K2_OnDestroy();
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
