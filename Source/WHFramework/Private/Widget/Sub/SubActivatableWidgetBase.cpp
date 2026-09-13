// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Sub/SubActivatableWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Widget/Screen/UserWidgetBase.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleTypes.h"

USubActivatableWidgetBase::USubActivatableWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;
	bDynamicSubWidget = false;

}

void USubActivatableWidgetBase::OnSpawn_Implementation(const FParameter& InParam)
{
	const FSubWidgetSpawnParameter* Param = InParam.GetPtr<FSubWidgetSpawnParameter>();
	OwnerWidget = Param ? Cast<IPanelWidgetInterface>(Param->OwnerObject) : nullptr;
	bDynamicSubWidget = Param ? Param->bDynamic : false;
}

void USubActivatableWidgetBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	OwnerWidget = nullptr;
	bDynamicSubWidget = false;
}

void USubActivatableWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
	
}

void USubActivatableWidgetBase::OnCreate(const FParameter& InParam)
{
	if(UWidgetModule::IsValid()) UWidgetModule::Get().RegisterTickableWidget(this);

	for(auto Iter : GetPoolWidgets())
	{
		IObjectPoolInterface::Execute_OnSpawn(Iter, FParameter(FWidgetSpawnParameter(this)));
	}

	K2_OnCreate(InParam);

	OnInitialize(InParam);
}

void USubActivatableWidgetBase::OnInitialize(const FParameter& InParam)
{
	WidgetParams = InParam;
	K2_OnInitialize(InParam);

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

void USubActivatableWidgetBase::OnDestroy(EObjectDespawnMode InMode)
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

void USubActivatableWidgetBase::NativeOnActivated()
{
	Super::NativeOnActivated();
}

void USubActivatableWidgetBase::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
}

void USubActivatableWidgetBase::Init(const FParameter& InParam)
{
	OnInitialize(InParam);
}

void USubActivatableWidgetBase::Reset(bool bForce)
{
	OnReset(bForce);
}

void USubActivatableWidgetBase::Refresh()
{
	OnRefresh();
}

void USubActivatableWidgetBase::Destroy(EObjectDespawnMode InMode)
{
	if(OwnerWidget)
	{
		OwnerWidget->DestroySubWidget(this, InMode);
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

TOptional<FUIInputConfig> USubActivatableWidgetBase::GetDesiredInputConfig() const
{
	switch(InputConfig)
	{
		case EWidgetInputConfig::Game:
			return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently, true);
		case EWidgetInputConfig::GameAndMenu:
			return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture, false);
		case EWidgetInputConfig::Menu:
			return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
		default:
			return TOptional<FUIInputConfig>();
	}
}
