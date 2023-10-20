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

void USubWidgetBase::OnCreate_Implementation(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams)
{
	Owner = InOwner;
	OnInitialize(InParams);
}

void USubWidgetBase::OnInitialize_Implementation(const TArray<FParameter>& InParams)
{
	Params = InParams;
	OnRefresh();
}

void USubWidgetBase::OnRefresh_Implementation()
{

}

void USubWidgetBase::OnDestroy_Implementation()
{
	
}