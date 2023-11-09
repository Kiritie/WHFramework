// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/World/WorldWidgetBase.h"

#include "Blueprint/WidgetLayoutLibrary.h"
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
	WidgetName = NAME_None;
	WidgetZOrder = 0;
	WidgetAnchors = FAnchors(0.f, 0.f, 0.f, 0.f);
	bWidgetAutoSize = false;
	WidgetDrawSize = FVector2D(0.f);
	WidgetOffsets = FMargin(0.f);
	WidgetAlignment = FVector2D(0.f);
	WidgetRefreshType = EWidgetRefreshType::Procedure;
	WidgetRefreshTime = 0;
	bWidgetAutoVisibility = false;
	WidgetShowDistance = -1;
	WidgetParams = TArray<FParameter>();
	WidgetInputMode = EInputMode::None;
	OwnerObject = nullptr;
	WidgetIndex = 0;

	WidgetComponent = nullptr;
	BindWidgetMap = TMap<UWidget*, FWorldWidgetBindInfo>();
}

void UWorldWidgetBase::OnTick_Implementation(float DeltaSeconds)
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
	if(bWidgetAutoVisibility)
	{
		RefreshVisibility();
	}
}

void UWorldWidgetBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
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

void UWorldWidgetBase::OnCreate(UObject* InOwner, FWorldWidgetBindInfo InBindInfo, const TArray<FParameter>& InParams)
{
	OwnerObject = InOwner;
	WidgetParams = InParams;
	
	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		GetWorld()->GetTimerManager().SetTimer(RefreshTimerHandle, this, &UWorldWidgetBase::Refresh, WidgetRefreshTime, true);
	}
	
	if(UInputModule* InputModule = UInputModule::Get())
	{
		InputModule->UpdateInputMode();
	}

	if(InBindInfo.SceneComp && InBindInfo.SceneComp->IsA<UWorldWidgetComponent>())
	{
		WidgetComponent = Cast<UWorldWidgetComponent>(InBindInfo.SceneComp);
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
					BindWidgetPoint(this, InBindInfo);
				}
				else if(WidgetComponent->IsBindToSelf())
				{
					InBindInfo.SceneComp = WidgetComponent;
					BindWidgetPoint(this, InBindInfo);
				}
			}
		}
	}

	if(bWidgetAutoVisibility)
	{
		RefreshVisibility();
	}

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

	if(UInputModule* InputModule = UInputModule::Get())
	{
		InputModule->UpdateInputMode();
	}

	UObjectPoolModuleStatics::DespawnObject(this, bRecovery);

	K2_OnDestroy(bRecovery);
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

void UWorldWidgetBase::RefreshVisibility_Implementation()
{
	bool bShow = false;
	FWorldWidgetBindInfo BindInfo;
	if(GetWidgetBindInfo(this, BindInfo))
	{
		const auto OwnerActor = Cast<AActor>(OwnerObject);
		const FVector Location = BindInfo.SceneComp ? (BindInfo.SocketName.IsNone() ? BindInfo.SceneComp->GetComponentLocation() : BindInfo.SceneComp->GetSocketLocation(BindInfo.SocketName)) + BindInfo.Location : BindInfo.Location;
		bShow = (OwnerActor ? OwnerActor->WasRecentlyRendered() : UCommonStatics::IsInScreenViewport(Location)) && (WidgetShowDistance == -1 || FVector::Distance(Location, UCameraModuleStatics::GetCameraLocation(true)) < WidgetShowDistance);
	}
	SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
}

void UWorldWidgetBase::RefreshLocation_Implementation(UWidget* InWidget, FWorldWidgetBindInfo InBindInfo)
{
	if(UCanvasPanelSlot* CanvasPanelSlot = Cast<UCanvasPanelSlot>(InWidget->Slot))
	{
		FVector2D ScreenPos;
		const FVector Location = InBindInfo.SceneComp ? (InBindInfo.SocketName.IsNone() ? InBindInfo.SceneComp->GetComponentLocation() : InBindInfo.SceneComp->GetSocketLocation(InBindInfo.SocketName)) + InBindInfo.Location : InBindInfo.Location;
		if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(UCommonStatics::GetPlayerController(), Location, ScreenPos, false))
		{
			CanvasPanelSlot->SetPosition(ScreenPos);
		}
	}
}

void UWorldWidgetBase::BindWidgetPoint_Implementation(UWidget* InWidget, FWorldWidgetBindInfo InBindInfo)
{
	if(!BindWidgetMap.Contains(InWidget))
	{
		BindWidgetMap.Add(InWidget, InBindInfo);
		RefreshLocation(InWidget, InBindInfo);
	}
}

void UWorldWidgetBase::UnBindWidgetPoint_Implementation(UWidget* InWidget)
{
	if(BindWidgetMap.Contains(InWidget))
	{
		BindWidgetMap.Remove(InWidget);
	}
}

bool UWorldWidgetBase::GetWidgetBindInfo_Implementation(UWidget* InWidget, FWorldWidgetBindInfo& OutBindInfo)
{
	if(BindWidgetMap.Contains(InWidget))
	{
		OutBindInfo = BindWidgetMap[InWidget];
		return true;
	}
	return false;
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
