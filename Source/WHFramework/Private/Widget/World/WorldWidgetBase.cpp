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
	bWidgetAutoSize = false;
	WidgetZOrder = -1;
	WidgetAnchors = FAnchors(0.f, 0.f, 1.f, 1.f);
	WidgetOffsets = FMargin(0.f);
	WidgetAlignment = FVector2D(0.f);
	WidgetRefreshType = EWidgetRefreshType::Tick;
	WidgetRefreshTime = 0;
	InputMode = EInputMode::None;
	OwnerActor = nullptr;
	WidgetIndex = 0;

	WidgetComponent = nullptr;
	BindWidgetMap = TMap<UWidget*, class USceneComponent*>();
}

void UWorldWidgetBase::TickWidget_Implementation()
{
	
}

void UWorldWidgetBase::OnCreate_Implementation(AActor* InOwner, FVector InLocation, USceneComponent* InSceneComp, const TArray<FParameter>& InParams)
{
	OwnerActor = InOwner;
	
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
		if(WidgetComponent->GetWidgetSpace() == EWidgetSpace::World) return;
	}

	if(!IsInViewport())
	{
		if(UWorldWidgetContainer* Container = UWidgetModuleBPLibrary::GetWorldWidgetContainer())
		{
			if(UCanvasPanelSlot* CanvasPanelSlot = Container->AddWorldWidget(this))
			{
				CanvasPanelSlot->SetZOrder(WidgetZOrder);
				CanvasPanelSlot->SetAnchors(WidgetAnchors);
				CanvasPanelSlot->SetAutoSize(bWidgetAutoSize);
				CanvasPanelSlot->SetOffsets(WidgetOffsets);
				CanvasPanelSlot->SetAlignment(WidgetAlignment);
				SetRenderTransformPivot(WidgetAlignment);

				if(!InLocation.IsNearlyZero())
				{
					FVector2D ScreenPos;
					if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(this), OwnerActor->GetActorLocation(), ScreenPos, false))
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
					else if(WidgetComponent->IsBindWidgetToSelf())
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

UPanelWidget* UWorldWidgetBase::GetRootPanelWidget() const
{
	return Cast<UPanelWidget>(GetRootWidget());
}
