// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/World/WorldWidgetBase.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Debug/DebugModuleTypes.h"
#include "Main/MainModule.h"
#include "Slate/SGameLayerManager.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleBPLibrary.h"
#include "Widget/World/WorldWidgetComponent.h"
#include "Widget/World/WorldWidgetContainer.h"

UWorldWidgetBase::UWorldWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = NAME_None;
	bWidgetTickAble = true;
	WidgetZOrder = -1;
	WidgetAnchors = FAnchors(0.f, 0.f, 0.f, 0.f);
	bWidgetAutoSize = false;
	WidgetDrawSize = FVector2D(0.f);
	WidgetOffsets = FMargin(0.f);
	WidgetAlignment = FVector2D(0.f);
	WidgetRefreshType = EWidgetRefreshType::Procedure;
	WidgetRefreshTime = 0;
	WidgetParams = TArray<FParameter>();
	InputMode = EInputMode::None;
	OwnerActor = nullptr;
	WidgetIndex = 0;

	WidgetComponent = nullptr;
	BindWidgetMap = TMap<UWidget*, class USceneComponent*>();
}

void UWorldWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
	if(GetWidgetSpace() == EWidgetSpace::Screen)
	{
		for(auto Iter : BindWidgetMap)
		{
			if(UCanvasPanelSlot* CanvasPanelSlot = Cast<UCanvasPanelSlot>(Iter.Key->Slot))
			{
				FVector2D ScreenPos;
				if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(this), Iter.Value->GetComponentLocation(), ScreenPos, false))
				{
					CanvasPanelSlot->SetPosition(ScreenPos);
				}
			}
		}
	}
}

void UWorldWidgetBase::OnCreate_Implementation(AActor* InOwner, FVector InLocation, USceneComponent* InSceneComp, const TArray<FParameter>& InParams)
{
	OwnerActor = InOwner;
	WidgetParams = InParams;
	
	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		GetWorld()->GetTimerManager().SetTimer(RefreshTimerHandle, this, &UWorldWidgetBase::Refresh, WidgetRefreshTime, true);
	}
	
	if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
	{
		InputModule->UpdateInputMode();
	}

	if(InSceneComp && InSceneComp->IsA(UWorldWidgetComponent::StaticClass()))
	{
		WidgetComponent = Cast<UWorldWidgetComponent>(InSceneComp);
	}

	if(GetWidgetSpace() == EWidgetSpace::Screen && !IsInViewport())
	{
		if(UWorldWidgetContainer* Container = UWidgetModuleBPLibrary::GetWorldWidgetContainer())
		{
			if(UCanvasPanelSlot* CanvasPanelSlot = Container->AddWorldWidget(this))
			{
				CanvasPanelSlot->SetZOrder(WidgetZOrder);
				CanvasPanelSlot->SetAutoSize(bWidgetAutoSize);
				if(!bWidgetAutoSize)
				{
					CanvasPanelSlot->SetAnchors(WidgetAnchors);
					CanvasPanelSlot->SetOffsets(WidgetOffsets);
				}
				CanvasPanelSlot->SetAlignment(WidgetAlignment);
				SetRenderTransformPivot(WidgetAlignment);

				if(!InLocation.IsNearlyZero())
				{
					FVector2D ScreenPos;
					if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(this), InLocation, ScreenPos, false))
					{
						CanvasPanelSlot->SetPosition(ScreenPos);
					}
				}
				else if(InSceneComp)
				{
					if(!WidgetComponent)
					{
						BindWidgetPoint(this, InSceneComp);
					}
					else if(WidgetComponent->IsBindToSelf())
					{
						BindWidgetPoint(this, WidgetComponent);
					}
				}
			}
		}
	}
}

void UWorldWidgetBase::OnRefresh_Implementation()
{
	
}

void UWorldWidgetBase::OnDestroy_Implementation()
{
	if(IsInViewport())
	{
		if(UWorldWidgetContainer* Container = UWidgetModuleBPLibrary::GetWorldWidgetContainer())
		{
			Container->RemoveWorldWidget(this);
		}
	}

	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		GetWorld()->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
	{
		InputModule->UpdateInputMode();
	}
}

void UWorldWidgetBase::Refresh_Implementation()
{
	if(WidgetRefreshType == EWidgetRefreshType::None) return;

	OnRefresh();
}

void UWorldWidgetBase::Destroy_Implementation()
{
	UWidgetModuleBPLibrary::DestroyWorldWidget<UWorldWidgetBase>(WidgetIndex, GetClass());
}

void UWorldWidgetBase::BindWidgetPoint_Implementation(UWidget* InWidget, USceneComponent* InSceneComp)
{
	if(!InSceneComp) return;
	
	if(!BindWidgetMap.Contains(InWidget))
	{
		BindWidgetMap.Add(InWidget, InSceneComp);
		if(UCanvasPanelSlot* CanvasPanelSlot = Cast<UCanvasPanelSlot>(InWidget->Slot))
		{
			FVector2D ScreenPos;
			if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(this), InSceneComp->GetComponentLocation(), ScreenPos, false))
			{
				CanvasPanelSlot->SetPosition(ScreenPos);
			}
		}
	}
}

void UWorldWidgetBase::BindWidgetPointByName_Implementation(UWidget* InWidget, FName InPointName)
{
	if(WidgetComponent)
	{
		BindWidgetPoint(InWidget, WidgetComponent->GetWidgetPoint(InPointName));
	}
}

void UWorldWidgetBase::UnBindWidgetPoint_Implementation(UWidget* InWidget)
{
	if(BindWidgetMap.Contains(InWidget))
	{
		BindWidgetMap.Remove(InWidget);
	}
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
