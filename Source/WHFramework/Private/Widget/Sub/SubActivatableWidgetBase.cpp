// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Sub/SubActivatableWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Widget/Screen/UserWidgetBase.h"

USubActivatableWidgetBase::USubActivatableWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;

	OwnerWidget = nullptr;
}

void USubActivatableWidgetBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void USubActivatableWidgetBase::OnDespawn_Implementation(bool bRecovery)
{
	
}

void USubActivatableWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
	
}

void USubActivatableWidgetBase::OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams)
{
	OwnerWidget = Cast<IPanelWidgetInterface>(InOwner);

	for(auto Iter : GetPoolWidgets())
	{
		IObjectPoolInterface::Execute_OnSpawn(Iter, nullptr, {});
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
	K2_OnDestroy(bRecovery);

	UObjectPoolModuleStatics::DespawnObject(this, bRecovery);

	OwnerWidget = nullptr;
	WidgetParams.Empty();
}

void USubActivatableWidgetBase::NativeOnActivated()
{
	Super::NativeOnActivated();

	OnActivated();
}

void USubActivatableWidgetBase::OnActivated()
{
}

void USubActivatableWidgetBase::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	OnDeactivated();
}

void USubActivatableWidgetBase::OnDeactivated()
{
}

bool USubActivatableWidgetBase::NativeOnHandleBackAction()
{
	if(!Super::NativeOnHandleBackAction()) return false;
	
	return OnHandleBackAction();
}

bool USubActivatableWidgetBase::OnHandleBackAction()
{
	return true;
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
