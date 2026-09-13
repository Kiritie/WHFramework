// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/World/WorldWidgetBase.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraModuleStatics.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Common/CommonModuleStatics.h"
#include "Scene/SceneManager.h"
#include "Slate/Runtime/Interfaces/SubWidgetInterface.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleStatics.h"
#include "Widget/World/WorldWidgetComponent.h"
#include "Widget/World/WorldWidgetContainer.h"

namespace
{
	FParameter MakeWorldSubWidgetParam(
		UUserWidget* InOwnerWidget,
		const FParameter& InParam,
		bool bInDynamic)
	{
		if(!InParam.HasValue())
		{
			return FParameter(FSubWidgetSpawnParameter(InOwnerWidget, bInDynamic));
		}

		FParameter Param = InParam;
		if(FSubWidgetSpawnParameter* SpawnParam = Param.GetMutablePtr<FSubWidgetSpawnParameter>())
		{
			SpawnParam->OwnerObject = InOwnerWidget;
			SpawnParam->bDynamic = bInDynamic;
		}
		else
		{
			ensureEditorMsgf(
				false,
				FString::Printf(
					TEXT("Sub widget parameter %s must derive from FSubWidgetSpawnParameter."),
					*GetNameSafe(Param.GetValueStruct())),
				EDC_Widget,
				EDV_Error);
		}
		return Param;
	}
}

UWorldWidgetBase::UWorldWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;
	WidgetTag = FGameplayTag();
	DefaultWidgetTag = FGameplayTag();
	WidgetRefreshType = EWidgetRefreshType::Procedure;
	WidgetRefreshTime = 0;
	InitializeParameter = FParameter();
	bInitialized = false;
	bWidgetInEditor = false;

	WidgetComponent = nullptr;
	BindWidgetMap = TMap<UWidget*, FWorldWidgetMapping>();
	SubWidgetEntries = TArray<FSubWidgetRuntimeEntry>();
}

const FWorldWidgetConfig* UWorldWidgetBase::GetWidgetConfig() const
{
	return UWidgetModule::IsValid()
		? UWidgetModule::Get().GetWorldWidgetConfig(WidgetTag)
		: nullptr;
}

int32 UWorldWidgetBase::GetWidgetZOrder() const
{
	const FWorldWidgetConfig* Config = GetWidgetConfig();
	return Config ? Config->ZOrder : 0;
}

FAnchors UWorldWidgetBase::GetWidgetAnchors() const
{
	const FWorldWidgetConfig* Config = GetWidgetConfig();
	return Config ? Config->Anchors : FAnchors(0.f, 0.f, 0.f, 0.f);
}

bool UWorldWidgetBase::IsWidgetAutoSize() const
{
	const FWorldWidgetConfig* Config = GetWidgetConfig();
	return Config ? Config->bAutoSize : false;
}

FVector2D UWorldWidgetBase::GetWidgetDrawSize() const
{
	const FWorldWidgetConfig* Config = GetWidgetConfig();
	return Config ? Config->DrawSize : FVector2D::ZeroVector;
}

FMargin UWorldWidgetBase::GetWidgetOffsets() const
{
	const FWorldWidgetConfig* Config = GetWidgetConfig();
	return Config ? Config->Offsets : FMargin(0.f);
}

FVector2D UWorldWidgetBase::GetWidgetAlignment() const
{
	const FWorldWidgetConfig* Config = GetWidgetConfig();
	return Config ? Config->Alignment : FVector2D::ZeroVector;
}

EWorldWidgetVisibility UWorldWidgetBase::GetWidgetVisibility() const
{
	const FWorldWidgetConfig* Config = GetWidgetConfig();
	return Config ? Config->Visibility : EWorldWidgetVisibility::AlwaysShow;
}

float UWorldWidgetBase::GetWidgetShowDistance() const
{
	const FWorldWidgetConfig* Config = GetWidgetConfig();
	return Config ? Config->ShowDistance : -1.f;
}

void UWorldWidgetBase::OnSpawn_Implementation(const FParameter& InParam)
{
	const FWidgetSpawnParameter* Param = InParam.GetPtr<FWidgetSpawnParameter>();
	OwnerObject = Param ? Param->OwnerObject.Get() : nullptr;
}

void UWorldWidgetBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	OwnerObject = nullptr;
}

void UWorldWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
}

void UWorldWidgetBase::OnCreate(FWorldWidgetMapping InMapping, const FParameter& InParam)
{
	if(UWidgetModule::IsValid()) UWidgetModule::Get().RegisterTickableWidget(this);

	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		GetWorld()->GetTimerManager().SetTimer(RefreshTimerHandle, this, &UWorldWidgetBase::Refresh, WidgetRefreshTime, true);
	}

	if(!IsWidgetInEditor())
	{
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

	K2_OnCreate(InParam);

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
					const FWorldWidgetConfig* Config = GetWidgetConfig();
					CanvasPanelSlot->SetZOrder(Config ? Config->ZOrder : 0);
					CanvasPanelSlot->SetAutoSize(Config ? Config->bAutoSize : false);
					CanvasPanelSlot->SetAnchors(Config ? Config->Anchors : FAnchors(0.f));
					if(!Config || !Config->bAutoSize)
					{
						CanvasPanelSlot->SetOffsets(Config ? Config->Offsets : FMargin(0.f));
					}
					const FVector2D Alignment = Config ? Config->Alignment : FVector2D::ZeroVector;
					CanvasPanelSlot->SetAlignment(Alignment);
					SetRenderTransformPivot(Alignment);
				}
			}
			RefreshVisibility();
		}
	}

	const FParameter ChildParam = FParameter(FSubWidgetSpawnParameter(this, false));
	for(auto Iter : GetPoolWidgets())
	{
		IObjectPoolInterface::Execute_OnSpawn(Iter, ChildParam);
	}
	
	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);
	for(auto Iter : Widgets)
	{
		if(ISubWidgetInterface* SubWidget = Cast<ISubWidgetInterface>(Iter))
		{
			FSubWidgetRuntimeEntry& Entry = SubWidgetEntries.AddDefaulted_GetRef();
			Entry.Widget = Cast<UUserWidget>(Iter);
			Entry.bDynamic = false;
			SubWidget->OnCreate(ChildParam);
		}
	}

	Init(InParam);
}

void UWorldWidgetBase::OnInitialize(const FParameter& InParam)
{
	InitializeParameter = InParam;
	K2_OnInitialize(InParam);

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

void UWorldWidgetBase::OnDestroy(EObjectDespawnMode InMode)
{
	if(UWidgetModule::IsValid()) UWidgetModule::Get().UnregisterTickableWidget(this);
	GetWorld()->GetTimerManager().ClearTimer(RefreshTimerHandle);
	DestroyAllSubWidget(InMode);
	for(UWidget* PoolWidget : GetPoolWidgets())
	{
		IObjectPoolInterface::Execute_OnDespawn(PoolWidget, InMode);
	}

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

	K2_OnDestroy(InMode);

	if(K2_OnDestroyed.IsBound()) K2_OnDestroyed.Broadcast(InMode);
	if(OnDestroyed.IsBound()) OnDestroyed.Broadcast(InMode);

	InitializeParameter.Reset();
	bInitialized = false;
	bWidgetInEditor = false;
	WidgetComponent = nullptr;
	WidgetTag = FGameplayTag();
	BindWidgetMap.Empty();
	UObjectPoolModuleStatics::DespawnObject(
		this,
		InMode);
}

void UWorldWidgetBase::Init(const FParameter& InParam, bool bForce)
{
	if(!bInitialized || bForce)
	{
		OnInitialize(InParam);
		bInitialized = true;
	}
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

void UWorldWidgetBase::Destroy(EObjectDespawnMode InMode)
{
	if(WidgetComponent)
	{
		WidgetComponent->DestroyWorldWidget(InMode, bWidgetInEditor);
	}
	else
	{
		UWidgetModuleStatics::DestroyWorldWidget(this, InMode);
	}
}

UUserWidget* UWorldWidgetBase::K2_CreateSubWidget(TSubclassOf<UUserWidget> InClass, const FParameter& InParam)
{
	return Cast<UUserWidget>(CreateSubWidget(InClass, InParam));
}

ISubWidgetInterface* UWorldWidgetBase::CreateSubWidget(TSubclassOf<UUserWidget> InClass, const FParameter& InParam)
{
	const FParameter Param = MakeWorldSubWidgetParam(this, InParam, true);
	if(ISubWidgetInterface* SubWidget = Cast<ISubWidgetInterface>(
		UObjectPoolModuleStatics::SpawnObject(InClass.Get(), Param)))
	{
		FSubWidgetRuntimeEntry& Entry = SubWidgetEntries.AddDefaulted_GetRef();
		Entry.Widget = Cast<UUserWidget>(SubWidget);
		Entry.bDynamic = true;
		SubWidget->OnCreate(Param);
		return SubWidget;
	}
	return nullptr;
}

bool UWorldWidgetBase::K2_DestroySubWidget(UUserWidget* InWidget, EObjectDespawnMode InMode)
{
	return DestroySubWidget(Cast<ISubWidgetInterface>(InWidget), InMode);
}

bool UWorldWidgetBase::DestroySubWidget(ISubWidgetInterface* InWidget, EObjectDespawnMode InMode)
{
	if(!InWidget) return false;

	const int32 EntryIndex = FindSubWidget(InWidget);
	if(EntryIndex != INDEX_NONE)
	{
		const FSubWidgetRuntimeEntry Entry = SubWidgetEntries[EntryIndex];
		SubWidgetEntries.RemoveAt(EntryIndex);
		InWidget->OnDestroy(InMode);
		if(!Entry.bDynamic && Entry.Widget)
		{
			IObjectPoolInterface::Execute_OnDespawn(Entry.Widget, InMode);
		}
		return true;
	}
	InWidget->OnDestroy(InMode);
	return true;
}

void UWorldWidgetBase::DestroyAllSubWidget(EObjectDespawnMode InMode)
{
	const TArray<FSubWidgetRuntimeEntry> Entries = MoveTemp(SubWidgetEntries);
	SubWidgetEntries.Reset();
	for(const FSubWidgetRuntimeEntry& Entry : Entries)
	{
		if(ISubWidgetInterface* SubWidget = Cast<ISubWidgetInterface>(Entry.Widget))
		{
			SubWidget->OnDestroy(InMode);
			if(!Entry.bDynamic)
			{
				IObjectPoolInterface::Execute_OnDespawn(Entry.Widget, InMode);
			}
		}
	}
}

void UWorldWidgetBase::RefreshLocation_Implementation(UWidget* InWidget, FWorldWidgetMapping InMapping)
{
	if(UCanvasPanelSlot* CanvasPanelSlot = Cast<UCanvasPanelSlot>(InWidget->Slot))
	{
		FVector2D ScreenPos;
		const FVector Location = InMapping.GetLocation();
		if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(UCommonModuleStatics::GetPlayerController(), Location, ScreenPos, false) && Location != FVector(-1.f))
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
		if(UCommonModuleStatics::GetLocalPlayerNum() == 1 && UWidgetModuleStatics::GetWorldWidgetVisible(GetClass()))
		{
			FWorldWidgetMapping Mapping;
			const auto OwnerActor = Cast<AActor>(OwnerObject);
			
			const FVector Location = GetWidgetMapping(this, Mapping) ? Mapping.GetLocation() : (OwnerActor ? OwnerActor->GetActorLocation() : FVector(-1.f));
			const FVector CameraLocation = UCameraModuleStatics::GetCameraLocation(true);
			const float Distance = FVector::Distance(Location, CameraLocation);
			const FWorldWidgetConfig* Config = GetWidgetConfig();
			const EWorldWidgetVisibility ConfigVisibility = Config ? Config->Visibility : EWorldWidgetVisibility::AlwaysShow;
			const float ShowDistance = Config ? Config->ShowDistance : -1.f;
			switch(ConfigVisibility)
			{
				case EWorldWidgetVisibility::AlwaysShow:
				{
					bVisible = true;
					break;
				}
				case EWorldWidgetVisibility::RenderOnly:
				case EWorldWidgetVisibility::ScreenOnly:
				case EWorldWidgetVisibility::DistanceOnly:
				case EWorldWidgetVisibility::RenderAndDistance:
				case EWorldWidgetVisibility::ScreenAndDistance:
				case EWorldWidgetVisibility::RenderScreenAndDistance:
				{
					bVisible = true;
					if(ENUMWITH(ConfigVisibility, EWorldWidgetVisibility::RenderOnly))
					{
						bVisible = bVisible && (!OwnerActor || !OwnerActor->IsHidden());
					}
					if(ENUMWITH(ConfigVisibility, EWorldWidgetVisibility::ScreenOnly))
					{
						bVisible = bVisible && (Location != FVector(-1.f) && UCommonModuleStatics::IsInScreenViewport(Location));
					}
					if(ENUMWITH(ConfigVisibility, EWorldWidgetVisibility::DistanceOnly))
					{
						bVisible = bVisible && (ShowDistance == -1 || (ShowDistance >= 0.f ? Distance < ShowDistance : Distance > FMath::Abs(ShowDistance)));
					}
					if(bVisible && ENUMWITH(ConfigVisibility, EWorldWidgetVisibility::RenderOnly))
					{
						bVisible = Location != FVector(-1.f);
						if(bVisible)
						{
							FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WorldWidgetVisibility), true, OwnerActor);
							if(Mapping.SceneComp) QueryParams.AddIgnoredActor(Mapping.SceneComp->GetOwner());
							QueryParams.AddIgnoredActor(UCommonModuleStatics::GetPlayerPawn());
							bVisible = !GetWorld()->LineTraceTestByChannel(CameraLocation, Location, UEngineTypes::ConvertToCollisionChannel(UWidgetModule::Get().GetWorldWidgetTraceType()), QueryParams);
						}
					}
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
	return GetWorldWidgetSpace() == EWorldWidgetSpace::World
		? EWidgetSpace::World
		: EWidgetSpace::Screen;
}

EWorldWidgetSpace UWorldWidgetBase::GetWorldWidgetSpace() const
{
	const FWorldWidgetConfig* Config = GetWidgetConfig();
	return Config ? Config->Space : EWorldWidgetSpace::Screen;
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
	for(const FSubWidgetRuntimeEntry& Entry : SubWidgetEntries)
	{
		ReturnValues.Add(GetDeterminesOutputObject(Entry.Widget.Get(), InClass));
	}
	return ReturnValues;
}

TArray<ISubWidgetInterface*> UWorldWidgetBase::GetSubWidgets()
{
	TArray<ISubWidgetInterface*> Result;
	Result.Reserve(SubWidgetEntries.Num());
	for(const FSubWidgetRuntimeEntry& Entry : SubWidgetEntries)
	{
		if(ISubWidgetInterface* SubWidget = Cast<ISubWidgetInterface>(Entry.Widget))
		{
			Result.Add(SubWidget);
		}
	}
	return Result;
}

UUserWidget* UWorldWidgetBase::GetSubWidget(int32 InIndex, TSubclassOf<UUserWidget> InClass) const
{
	return GetDeterminesOutputObject(Cast<UUserWidget>(GetSubWidget(InIndex)), InClass);
}

int32 UWorldWidgetBase::FindSubWidget(UUserWidget* InWidget) const
{
	return FindSubWidget(Cast<ISubWidgetInterface>(InWidget));
}
