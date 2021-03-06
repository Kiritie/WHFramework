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
	WidgetZOrder = 0;
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
	BindWidgetMap = TMap<UWidget*, FWorldWidgetBindInfo>();
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
				if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(), Iter.Value.SceneComp ? Iter.Value.SceneComp->GetComponentLocation() + Iter.Value.Location : Iter.Value.Location, ScreenPos, false))
				{
					CanvasPanelSlot->SetPosition(ScreenPos);
				}
			}
		}
	}

}

void UWorldWidgetBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	
}

void UWorldWidgetBase::OnDespawn_Implementation()
{
	OwnerActor = nullptr;
	WidgetParams.Empty();
	WidgetIndex = 0;
	WidgetComponent = nullptr;
	BindWidgetMap.Empty();
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

	if(InSceneComp && InSceneComp->IsA<UWorldWidgetComponent>())
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

				if(!WidgetComponent)
				{
					BindWidgetPoint(this, InLocation, InSceneComp);
				}
				else if(WidgetComponent->IsBindToSelf())
				{
					BindWidgetPoint(this, InLocation, WidgetComponent);
				}
			}
		}
	}
}

void UWorldWidgetBase::OnRefresh_Implementation()
{
	
}

void UWorldWidgetBase::OnDestroy_Implementation(bool bRecovery)
{
	if(UWorldWidgetContainer* Container = UWidgetModuleBPLibrary::GetWorldWidgetContainer())
	{
		Container->RemoveWorldWidget(this);
	}

	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		GetWorld()->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
	{
		InputModule->UpdateInputMode();
	}

	if(bRecovery)
	{
		UObjectPoolModuleBPLibrary::DespawnObject(this);
	}
	else
	{
		ConditionalBeginDestroy();
	}
}

void UWorldWidgetBase::Refresh_Implementation()
{
	if(WidgetRefreshType == EWidgetRefreshType::None) return;

	OnRefresh();
}

void UWorldWidgetBase::Destroy_Implementation(bool bRecovery)
{
	UWidgetModuleBPLibrary::DestroyWorldWidget<UWorldWidgetBase>(WidgetIndex, bRecovery, GetClass());
}

void UWorldWidgetBase::BindWidgetPoint_Implementation(UWidget* InWidget, FVector InLocation, USceneComponent* InSceneComp)
{
	if(!InSceneComp) return;
	
	if(!BindWidgetMap.Contains(InWidget))
	{
		BindWidgetMap.Add(InWidget, FWorldWidgetBindInfo(InLocation, InSceneComp));
		if(UCanvasPanelSlot* CanvasPanelSlot = Cast<UCanvasPanelSlot>(InWidget->Slot))
		{
			FVector2D ScreenPos;
			if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(), InSceneComp ? InSceneComp->GetComponentLocation() + InLocation : InLocation, ScreenPos, false))
			{
				CanvasPanelSlot->SetPosition(ScreenPos);
			}
		}
	}
}

void UWorldWidgetBase::BindWidgetPointByName_Implementation(UWidget* InWidget, FVector InLocation, FName InPointName)
{
	if(WidgetComponent)
	{
		BindWidgetPoint(InWidget, InLocation, WidgetComponent->GetWidgetPoint(InPointName));
	}
}

void UWorldWidgetBase::UnBindWidgetPoint_Implementation(UWidget* InWidget)
{
	if(BindWidgetMap.Contains(InWidget))
	{
		BindWidgetMap.Remove(InWidget);
	}
}

FWorldWidgetBindInfo UWorldWidgetBase::GetWidgetBindInfo_Implementation(UWidget* InWidget)
{
	if(BindWidgetMap.Contains(InWidget))
	{
		return BindWidgetMap[InWidget];
	}
	return FWorldWidgetBindInfo();
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
