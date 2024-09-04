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
	bWidgetPenetrable = false;
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

FReply UWorldWidgetBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if(!bWidgetPenetrable)
	{
		Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
		if(IsWidgetInEditor())
		{
#if WITH_EDITOR
			if(AActor* OwnerActor = GetOwnerObject<AActor>())
			{
				FSceneManager::Get().SelectActorsInCurrentWorld({ OwnerActor }, !InMouseEvent.IsControlDown());
			}
#endif
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UWorldWidgetBase::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if(!bWidgetPenetrable)
	{
		Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UWorldWidgetBase::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if(!bWidgetPenetrable)
	{
		Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UWorldWidgetBase::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if(!bWidgetPenetrable)
	{
		Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UWorldWidgetBase::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if(!bWidgetPenetrable)
	{
		Super::NativeOnMouseMove(InGeometry, InMouseEvent);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UWorldWidgetBase::NativeOnTouchGesture(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if(!bWidgetPenetrable)
	{
		Super::NativeOnTouchGesture(InGeometry, InGestureEvent);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UWorldWidgetBase::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if(!bWidgetPenetrable)
	{
		Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UWorldWidgetBase::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if(!bWidgetPenetrable)
	{
		Super::NativeOnTouchMoved(InGeometry, InGestureEvent);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UWorldWidgetBase::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if(!bWidgetPenetrable)
	{
		Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void UWorldWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
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

void UWorldWidgetBase::OnCreate(UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams)
{
	OwnerObject = InOwner;
	WidgetParams = InParams;
	
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

	for(auto Iter : GetAllPoolWidgets())
	{
		IObjectPoolInterface::Execute_OnSpawn(Iter, nullptr, {});
	}
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

void UWorldWidgetBase::Refresh_Implementation()
{
	if(WidgetRefreshType == EWidgetRefreshType::None) return;

	OnRefresh();
}

void UWorldWidgetBase::Destroy_Implementation(bool bRecovery)
{
	UWidgetModuleStatics::DestroyWorldWidget<UWorldWidgetBase>(WidgetIndex, bRecovery, IsWidgetInEditor(), GetClass());
}

void UWorldWidgetBase::RefreshLocation_Implementation(UWidget* InWidget, FWorldWidgetMapping InMapping)
{
	if(UCanvasPanelSlot* CanvasPanelSlot = Cast<UCanvasPanelSlot>(InWidget->Slot))
	{
		FVector2D ScreenPos;
		const FVector Location = InMapping.GetLocation();
		if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(UCommonStatics::GetPlayerController(), Location, ScreenPos, false))
		{
			CanvasPanelSlot->SetPosition(ScreenPos);
		}
	}
}

void UWorldWidgetBase::RefreshVisibility_Implementation()
{
	SetVisibility(IsWidgetVisible(true) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
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
		FWorldWidgetMapping Mapping;
		if(GetWidgetMapping(this, Mapping) && UCommonStatics::GetLocalPlayerNum() == 1 && UWidgetModuleStatics::GetWorldWidgetVisible(false, GetClass()))
		{
			const auto OwnerActor = Cast<AActor>(OwnerObject);
			const FVector Location = Mapping.GetLocation();
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
					bVisible = UCommonStatics::IsInScreenViewport(Location);
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
					bVisible = UCommonStatics::IsInScreenViewport(Location) && (WidgetShowDistance == -1 || (WidgetShowDistance >= 0.f ? Distance < WidgetShowDistance : Distance > FMath::Abs(WidgetShowDistance)));
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

TArray<UWidget*> UWorldWidgetBase::GetAllPoolWidgets() const
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
