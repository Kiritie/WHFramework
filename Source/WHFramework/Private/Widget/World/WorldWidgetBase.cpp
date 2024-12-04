// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/World/WorldWidgetBase.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraModuleStatics.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Common/CommonStatics.h"
#include "Input/InputModuleStatics.h"
#include "Scene/SceneManager.h"
#include "Slate/Runtime/Interfaces/SubWidgetInterface.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleStatics.h"
#include "Widget/World/WorldWidgetComponent.h"
#include "Widget/World/WorldWidgetContainer.h"

UWorldWidgetBase::UWorldWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;
	WidgetName = NAME_None;
	WidgetSpace = EWidgetSpace::Screen;
	WidgetZOrder = 0;
	WidgetAnchors = FAnchors(0.f, 0.f, 0.f, 0.f);
	bWidgetAutoSize = false;
	WidgetDrawSize = FVector2D(0.f);
	WidgetOffsets = FMargin(0.f);
	WidgetAlignment = FVector2D(0.f);
	WidgetRefreshType = EWidgetRefreshType::Procedure;
	WidgetRefreshTime = 0;
	WidgetVisibility = EWorldWidgetVisibility::AlwaysShow;
	WidgetShowDistance = -1;
	WidgetParams = TArray<FParameter>();
	WidgetInputMode = EInputMode::None;
	OwnerObject = nullptr;
	WidgetIndex = 0;
	bWidgetInEditor = false;

	WidgetComponent = nullptr;
	BindWidgetMap = TMap<UWidget*, FWorldWidgetMapping>();
}

void UWorldWidgetBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void UWorldWidgetBase::OnDespawn_Implementation(bool bRecovery)
{
	OwnerObject = nullptr;
	WidgetParams.Empty();
	WidgetIndex = 0;
	WidgetComponent = nullptr;
	BindWidgetMap.Empty();
}

void UWorldWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
}

void UWorldWidgetBase::OnCreate(UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams)
{
	OwnerObject = InOwner;
	
	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		GetWorld()->GetTimerManager().SetTimer(RefreshTimerHandle, this, &UWorldWidgetBase::Refresh, WidgetRefreshTime, true);
	}

	if(!IsWidgetInEditor())
	{
		UInputModuleStatics::UpdateGlobalInputMode();
	}

	if(InMapping.SceneComp && InMapping.SceneComp->IsA<UWorldWidgetComponent>())
	{
		WidgetComponent = Cast<UWorldWidgetComponent>(InMapping.SceneComp);
	}

	if(!WidgetComponent)
	{
		BindWidgetPoint(this, InMapping);
	}
	else if(WidgetComponent->IsBindToSelf())
	{
		InMapping.SceneComp = WidgetComponent;
		BindWidgetPoint(this, InMapping);
	}

	K2_OnCreate(InOwner, InParams);

	Refresh();

	if(GetWidgetSpace() == EWidgetSpace::Screen)
	{
		if(IsWidgetInEditor())
		{
#if WITH_EDITOR
			if (WorldWidget.IsValid())
			{
				FSceneManager::Get().RemoveWidgetFromAllViewport(WorldWidget.ToSharedRef());
			}

			WorldWidget = TakeWidget();

			FSceneManager::Get().AddWidgetToAllViewport(WorldWidget.ToSharedRef());
#endif
		}
		else if(!IsInViewport())
		{
			if(UWorldWidgetContainer* Container = UWidgetModuleStatics::GetWorldWidgetContainer())
			{
				if(UCanvasPanelSlot* CanvasPanelSlot = Container->AddWorldWidget(this))
				{
					CanvasPanelSlot->SetZOrder(WidgetZOrder);
					CanvasPanelSlot->SetAutoSize(bWidgetAutoSize);
					CanvasPanelSlot->SetAnchors(WidgetAnchors);
					if(!bWidgetAutoSize)
					{
						CanvasPanelSlot->SetOffsets(WidgetOffsets);
					}
					CanvasPanelSlot->SetAlignment(WidgetAlignment);
					SetRenderTransformPivot(WidgetAlignment);
				}
			}
			RefreshVisibility();
		}
	}

	for(auto Iter : GetPoolWidgets())
	{
		IObjectPoolInterface::Execute_OnSpawn(Iter, nullptr, {});
	}
	
	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);
	for(auto Iter : Widgets)
	{
		if(ISubWidgetInterface* SubWidget = Cast<ISubWidgetInterface>(Iter))
		{
			SubWidgets.Add(SubWidget);
			SubWidget->OnCreate(this, SubWidget->GetWidgetParams());
		}
	}

	OnInitialize(InParams);
}

void UWorldWidgetBase::OnInitialize(const TArray<FParameter>& InParams)
{
	WidgetParams = InParams;
	K2_OnInitialize(InParams);

	OnRefresh();
}

void UWorldWidgetBase::OnReset(bool bForce)
{
	K2_OnReset(bForce);
}

void UWorldWidgetBase::OnRefresh()
{
	K2_OnRefresh();
}

void UWorldWidgetBase::OnDestroy(bool bRecovery)
{
	if(IsWidgetInEditor())
	{
#if WITH_EDITOR
		if (WorldWidget.IsValid())
		{
			FSceneManager::Get().RemoveWidgetFromAllViewport(WorldWidget.ToSharedRef());

			WorldWidget.Reset();
		}
#endif
	}
	else if(UWorldWidgetContainer* Container = UWidgetModuleStatics::GetWorldWidgetContainer())
	{
		Container->RemoveWorldWidget(this);
	}

	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		GetWorld()->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	
	UInputModuleStatics::UpdateGlobalInputMode();
	
	if(K2_OnDestroyed.IsBound()) K2_OnDestroyed.Broadcast(bRecovery);
	if(OnDestroyed.IsBound()) OnDestroyed.Broadcast(bRecovery);

	UObjectPoolModuleStatics::DespawnObject(this, bRecovery);

	K2_OnDestroy(bRecovery);
}

void UWorldWidgetBase::Init(const TArray<FParameter>* InParams)
{
	Init(InParams ? *InParams : TArray<FParameter>());
}

void UWorldWidgetBase::Init(const TArray<FParameter>& InParams)
{
	OnInitialize(InParams);
}

void UWorldWidgetBase::Reset(bool bForce)
{
	OnReset(bForce);
}

void UWorldWidgetBase::Refresh()
{
	if(WidgetRefreshType == EWidgetRefreshType::None) return;

	OnRefresh();
}

void UWorldWidgetBase::Destroy(bool bRecovery)
{
	UWidgetModuleStatics::DestroyWorldWidget<UWorldWidgetBase>(WidgetIndex, bRecovery, GetClass());
}

UUserWidget* UWorldWidgetBase::K2_CreateSubWidget(TSubclassOf<UUserWidget> InClass, const TArray<FParameter>& InParams)
{
	return Cast<UUserWidget>(CreateSubWidget(InClass, InParams));
}

ISubWidgetInterface* UWorldWidgetBase::CreateSubWidget(TSubclassOf<UUserWidget> InClass, const TArray<FParameter>* InParams)
{
	return CreateSubWidget(InClass, InParams ? *InParams : TArray<FParameter>());
}

ISubWidgetInterface* UWorldWidgetBase::CreateSubWidget(TSubclassOf<UUserWidget> InClass, const TArray<FParameter>& InParams)
{
	if(ISubWidgetInterface* SubWidget = UObjectPoolModuleStatics::SpawnObject<ISubWidgetInterface>(nullptr, nullptr, InClass))
	{
		SubWidget->OnCreate(this, InParams);
		return SubWidget;
	}
	return nullptr;
}

bool UWorldWidgetBase::K2_DestroySubWidget(UUserWidget* InWidget, bool bRecovery)
{
	return DestroySubWidget(Cast<ISubWidgetInterface>(InWidget), bRecovery);
}

bool UWorldWidgetBase::DestroySubWidget(ISubWidgetInterface* InWidget, bool bRecovery)
{
	if(!InWidget) return false;

	InWidget->OnDestroy(bRecovery);

	UObjectPoolModuleStatics::DespawnObject(Cast<UUserWidget>(InWidget), bRecovery);
	return true;
}

void UWorldWidgetBase::DestroyAllSubWidget(bool bRecovery)
{
	for(auto Iter : SubWidgets)
	{
		Iter->Destroy();
	}
	SubWidgets.Empty();
}

void UWorldWidgetBase::RefreshLocation_Implementation(UWidget* InWidget, FWorldWidgetMapping InMapping)
{
	if(UCanvasPanelSlot* CanvasPanelSlot = Cast<UCanvasPanelSlot>(InWidget->Slot))
	{
		FVector2D ScreenPos;
		const FVector Location = InMapping.GetLocation();
		if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(UCommonStatics::GetPlayerController(), Location, ScreenPos, false) && Location != FVector(-1.f))
		{
			CanvasPanelSlot->SetPosition(ScreenPos);
		}
	}
}

void UWorldWidgetBase::RefreshVisibility_Implementation()
{
	SetVisibility(IsWidgetVisible(true) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
}

void UWorldWidgetBase::RefreshLocationAndVisibility_Implementation()
{
	if(GetWidgetSpace() == EWidgetSpace::Screen)
	{
		if(BindWidgetMap.Contains(this))
		{
			RefreshLocation(this, BindWidgetMap[this]);
		}
		if(BindWidgetMap.Num() > 1)
		{
			for(const auto& Iter : BindWidgetMap)
			{
				if(Iter.Key == this) continue;
				RefreshLocation(Iter.Key, Iter.Value);
			}
		}
		RefreshVisibility();
	}
}

void UWorldWidgetBase::BindWidgetPoint_Implementation(UWidget* InWidget, FWorldWidgetMapping InMapping)
{
	if(!BindWidgetMap.Contains(InWidget))
	{
		BindWidgetMap.Add(InWidget, InMapping);
		RefreshLocation(InWidget, InMapping);
	}
}

void UWorldWidgetBase::UnBindWidgetPoint_Implementation(UWidget* InWidget)
{
	if(BindWidgetMap.Contains(InWidget))
	{
		BindWidgetMap.Remove(InWidget);
	}
}

bool UWorldWidgetBase::GetWidgetMapping_Implementation(UWidget* InWidget, FWorldWidgetMapping& OutMapping)
{
	if(BindWidgetMap.Contains(InWidget))
	{
		OutMapping = BindWidgetMap[InWidget];
		return true;
	}
	return false;
}

bool UWorldWidgetBase::IsWidgetVisible_Implementation(bool bRefresh)
{
	if(bRefresh)
	{
		bool bVisible = false;
		if(UCommonStatics::GetLocalPlayerNum() == 1 && UWidgetModuleStatics::GetWorldWidgetVisible(GetClass()))
		{
			FWorldWidgetMapping Mapping;
			const auto OwnerActor = Cast<AActor>(OwnerObject);
			
			const FVector Location = GetWidgetMapping(this, Mapping) ? Mapping.GetLocation() : (OwnerActor ? OwnerActor->GetActorLocation() : FVector(-1.f));
			const float Distance = FVector::Distance(Location, UCameraModuleStatics::GetCameraLocation(true));
			switch(WidgetVisibility)
			{
				case EWorldWidgetVisibility::AlwaysShow:
				{
					bVisible = true;
					break;
				}
				case EWorldWidgetVisibility::RenderOnly:
				{
					bVisible = !OwnerActor || OwnerActor->WasRecentlyRendered();
					break;
				}
				case EWorldWidgetVisibility::ScreenOnly:
				{
					bVisible = UCommonStatics::IsInScreenViewport(Location) && Location != FVector(-1.f);
					break;
				}
				case EWorldWidgetVisibility::DistanceOnly:
				{
					bVisible = WidgetShowDistance == -1 || (WidgetShowDistance >= 0.f ? Distance < WidgetShowDistance : Distance > FMath::Abs(WidgetShowDistance));
					break;
				}
				case EWorldWidgetVisibility::RenderAndDistance:
				{
					bVisible = (!OwnerActor || OwnerActor->WasRecentlyRendered()) && (WidgetShowDistance == -1 || (WidgetShowDistance >= 0.f ? Distance < WidgetShowDistance : Distance > FMath::Abs(WidgetShowDistance)));
					break;
				}
				case EWorldWidgetVisibility::ScreenAndDistance:
				{
					bVisible = UCommonStatics::IsInScreenViewport(Location) && Location != FVector(-1.f) && (WidgetShowDistance == -1 || (WidgetShowDistance >= 0.f ? Distance < WidgetShowDistance : Distance > FMath::Abs(WidgetShowDistance)));
					break;
				}
				default: break;
			}
		}
		return bVisible;
	}
	return GetVisibility() == ESlateVisibility::SelfHitTestInvisible;
}

EWidgetSpace UWorldWidgetBase::GetWidgetSpace() const
{
	return WidgetSpace;
}

UPanelWidget* UWorldWidgetBase::GetRootPanelWidget() const
{
	return Cast<UPanelWidget>(GetRootWidget());
}

TArray<UWidget*> UWorldWidgetBase::GetPoolWidgets() const
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

TArray<UUserWidget*> UWorldWidgetBase::K2_GetSubWidgets(TSubclassOf<UUserWidget> InClass)
{
	TArray<UUserWidget*> ReturnValues;
	for(auto Iter : SubWidgets)
	{
		ReturnValues.Add(GetDeterminesOutputObject(Cast<UUserWidget>(Iter), InClass));
	}
	return ReturnValues;
}

UUserWidget* UWorldWidgetBase::GetSubWidget(int32 InIndex, TSubclassOf<UUserWidget> InClass) const
{
	return GetDeterminesOutputObject(Cast<UUserWidget>(GetSubWidget(InIndex)), InClass);
}

int32 UWorldWidgetBase::FindSubWidget(UUserWidget* InWidget) const
{
	return FindSubWidget(Cast<ISubWidgetInterface>(InWidget));
}
