// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Sub/SubWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Widget/Screen/UserWidgetBase.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleTypes.h"

USubWidgetBase::USubWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;
	bDynamicSubWidget = false;

}

void USubWidgetBase::OnSpawn_Implementation(const FParameter& InParam)
{
	const FSubWidgetSpawnParameter* Param = InParam.GetPtr<FSubWidgetSpawnParameter>();
	OwnerWidget = Param ? Cast<IPanelWidgetInterface>(Param->OwnerObject) : nullptr;
	bDynamicSubWidget = Param ? Param->bDynamic : false;
}

void USubWidgetBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	OwnerWidget = nullptr;
	bDynamicSubWidget = false;
}

void USubWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
	
}

void USubWidgetBase::OnCreate(const FParameter& InParam)
{
	if(UWidgetModule::IsValid()) UWidgetModule::Get().RegisterTickableWidget(this);

	for(auto Iter : GetPoolWidgets())
	{
		IObjectPoolInterface::Execute_OnSpawn(Iter, FParameter(FWidgetSpawnParameter(this)));
	}

	K2_OnCreate(InParam);

	OnInitialize(InParam);
}

void USubWidgetBase::OnInitialize(const FParameter& InParam)
{
	WidgetParams = InParam;
	K2_OnInitialize(InParam);

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

void USubWidgetBase::OnDestroy(EObjectDespawnMode InMode)
{
	if(UWidgetModule::IsValid()) UWidgetModule::Get().UnregisterTickableWidget(this);
	for(UWidget* PoolWidget : GetPoolWidgets())
	{
		IObjectPoolInterface::Execute_OnDespawn(PoolWidget, InMode);
	}

	K2_OnDestroy(InMode);
	WidgetParams.Reset();

	if(bDynamicSubWidget)
	{
		RemoveFromParent();
		UObjectPoolModuleStatics::DespawnObject(this, InMode);
	}

}

void USubWidgetBase::Init(const FParameter& InParam)
{
	OnInitialize(InParam);
}

void USubWidgetBase::Reset(bool bForce)
{
	OnReset(bForce);
}

void USubWidgetBase::Refresh()
{
	OnRefresh();
}

void USubWidgetBase::Destroy(EObjectDespawnMode InMode)
{
	if(OwnerWidget)
	{
		OwnerWidget->DestroySubWidget(this, InMode);
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
