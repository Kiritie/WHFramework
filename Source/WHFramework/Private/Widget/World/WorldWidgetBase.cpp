// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/World/WorldWidgetBase.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraModuleStatics.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Common/CommonStatics.h"
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

	WidgetComponent = nullptr;
	BindWidgetMap = TMap<UWidget*, FWorldWidgetMapping>();
}

FReply UWorldWidgetBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UWorldWidgetBase::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UWorldWidgetBase::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

FReply UWorldWidgetBase::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

FReply UWorldWidgetBase::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UWorldWidgetBase::NativeOnTouchGesture(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : Super::NativeOnTouchGesture(InGeometry, InGestureEvent);
}

FReply UWorldWidgetBase::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
}

FReply UWorldWidgetBase::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : Super::NativeOnTouchMoved(InGeometry, InGestureEvent);
}

FReply UWorldWidgetBase::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
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
	
	UInputModule::Get().UpdateInputMode();

	if(InMapping.SceneComp && InMapping.SceneComp->IsA<UWorldWidgetComponent>())
	{
		WidgetComponent = Cast<UWorldWidgetComponent>(InMapping.SceneComp);
	}

	if(GetWidgetSpace() == EWidgetSpace::Screen && !IsInViewport())
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

				if(!WidgetComponent)
				{
					BindWidgetPoint(this, InMapping);
				}
				else if(WidgetComponent->IsBindToSelf())
				{
					InMapping.SceneComp = WidgetComponent;
					BindWidgetPoint(this, InMapping);
				}
			}
		}
	}

	for(auto Iter : GetAllPoolWidgets())
	{
		IObjectPoolInterface::Execute_OnSpawn(Iter, nullptr, {});
	}

	RefreshVisibility();

	K2_OnCreate(InOwner, InParams);
}

void UWorldWidgetBase::OnRefresh()
{
	K2_OnRefresh();
}

void UWorldWidgetBase::OnDestroy(bool bRecovery)
{
	if(UWorldWidgetContainer* Container = UWidgetModuleStatics::GetWorldWidgetContainer())
	{
		Container->RemoveWorldWidget(this);
	}

	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		GetWorld()->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	UInputModule::Get().UpdateInputMode();

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
	}
	RefreshVisibility();
}

void UWorldWidgetBase::Refresh_Implementation()
{
	if(WidgetRefreshType == EWidgetRefreshType::None) return;

	OnRefresh();
}

void UWorldWidgetBase::Destroy_Implementation(bool bRecovery)
{
	UWidgetModuleStatics::DestroyWorldWidget<UWorldWidgetBase>(WidgetIndex, bRecovery, GetClass());
}

void UWorldWidgetBase::RefreshLocation_Implementation(UWidget* InWidget, FWorldWidgetMapping InMapping)
{
	if(UCanvasPanelSlot* CanvasPanelSlot = Cast<UCanvasPanelSlot>(InWidget->Slot))
	{
		FVector2D ScreenPos;
		const FVector Location = InMapping.SceneComp ? (InMapping.SocketName.IsNone() ? InMapping.SceneComp->GetComponentLocation() : InMapping.SceneComp->GetSocketLocation(InMapping.SocketName)) + InMapping.Location : InMapping.Location;
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
		if(GetWidgetMapping(this, Mapping) && UCommonStatics::GetLocalPlayerNum() == 1)
		{
			const auto OwnerActor = Cast<AActor>(OwnerObject);
			const FVector Location = Mapping.SceneComp ? (Mapping.SocketName.IsNone() ? Mapping.SceneComp->GetComponentLocation() : Mapping.SceneComp->GetSocketLocation(Mapping.SocketName)) + Mapping.Location : Mapping.Location;

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
					bVisible = WidgetShowDistance == -1 || FVector::Distance(Location, UCameraModuleStatics::GetCameraLocation(true)) < WidgetShowDistance;
					break;
				}
				case EWorldWidgetVisibility::RenderAndDistance:
				{
					bVisible = (!OwnerActor || OwnerActor->WasRecentlyRendered()) && (WidgetShowDistance == -1 || FVector::Distance(Location, UCameraModuleStatics::GetCameraLocation(true)) < WidgetShowDistance);
					break;
				}
				case EWorldWidgetVisibility::ScreenAndDistance:
				{
					bVisible = UCommonStatics::IsInScreenViewport(Location) && (WidgetShowDistance == -1 || FVector::Distance(Location, UCameraModuleStatics::GetCameraLocation(true)) < WidgetShowDistance);
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
	if(WidgetComponent)
	{
		return WidgetComponent->GetWidgetSpace();
	}
	return EWidgetSpace::Screen;
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
