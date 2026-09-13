// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Sub/SubButtonWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Widget/Screen/UserWidgetBase.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleTypes.h"

USubButtonWidgetBase::USubButtonWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;
	bDynamicSubWidget = false;

}

void USubButtonWidgetBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
	const FSubWidgetSpawnParameter* Param = InParam.GetPtr<FSubWidgetSpawnParameter>();
	OwnerWidget = Param ? Cast<IPanelWidgetInterface>(Param->OwnerObject) : nullptr;
	bDynamicSubWidget = Param ? Param->bDynamic : false;
}

void USubButtonWidgetBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);
	OwnerWidget = nullptr;
	bDynamicSubWidget = false;
}

void USubButtonWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
	
}

void USubButtonWidgetBase::OnCreate(const FParameter& InParam)
{
	if(UWidgetModule::IsValid()) UWidgetModule::Get().RegisterTickableWidget(this);

	for(auto Iter : GetPoolWidgets())
	{
		IObjectPoolInterface::Execute_OnSpawn(Iter, FParameter(FWidgetSpawnParameter(this)));
	}

	K2_OnCreate(InParam);

	OnInitialize(InParam);
}

void USubButtonWidgetBase::OnInitialize(const FParameter& InParam)
{
	WidgetParams = InParam;
	K2_OnInitialize(InParam);

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

void USubButtonWidgetBase::OnDestroy(EObjectDespawnMode InMode)
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

void USubButtonWidgetBase::Init(const FParameter& InParam)
{
	OnInitialize(InParam);
}

void USubButtonWidgetBase::Reset(bool bForce)
{
	OnReset(bForce);
}

void USubButtonWidgetBase::Refresh()
{
	OnRefresh();
}

void USubButtonWidgetBase::Destroy(EObjectDespawnMode InMode)
{
	if(OwnerWidget)
	{
		OwnerWidget->DestroySubWidget(this, InMode);
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
