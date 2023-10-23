// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Screen/UMG/SubWidgetBase.h"

#include "Widget/Screen/UMG/UserWidgetBase.h"

USubWidgetBase::USubWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Owner = nullptr;
}

void USubWidgetBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{

}

void USubWidgetBase::OnDespawn_Implementation(bool bRecovery)
{
	Owner = nullptr;
	RemoveFromParent();
}

void USubWidgetBase::OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams)
{
	Owner = InOwner;
	K2_OnCreate(InOwner, InParams);

	OnInitialize(InParams);
}

void USubWidgetBase::OnInitialize(const TArray<FParameter>& InParams)
{
	Params = InParams;
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
	if(Owner)
	{
		Owner->DestroySubWidget(this, bRecovery);
	}
}
