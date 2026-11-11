// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/World/WorldWidgetComponent.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "Widget/WorldWidgetTypes.h"

#include "Camera/CameraModuleStatics.h"
#include "Common/CommonModuleStatics.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Scene/SceneManager.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleStatics.h"
#include "Widget/World/WorldWidgetActor.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	FParameter MakeWidgetParam(const UWorldWidgetComponent* InComponent, const FParameter& InParam)
	{
		UObject* OwnerObject = InComponent ? InComponent->GetOwner() : nullptr;
		if(!InParam.HasValue())
		{
			return FParameter(FWidgetSpawnParameter(OwnerObject));
		}

		FParameter Param = InParam;
		FWidgetSpawnParameter* SpawnParam = Param.GetMutablePtr<FWidgetSpawnParameter>();
		if(!SpawnParam)
		{
			ensureEditorMsgf(
				false,
				FString::Printf(
					TEXT("World widget parameter %s must derive from FWidgetSpawnParameter."),
					*GetNameSafe(Param.GetValueStruct())),
				EDC_Widget,
				EDV_Error);
			return FParameter(FWidgetSpawnParameter(OwnerObject));
		}

		SpawnParam->OwnerObject = OwnerObject;
		return Param;
	}
}

UWorldWidgetComponent::UWorldWidgetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	UPrimitiveComponent::SetCollisionEnabled(ECollisionEnabled::NoCollision);

#if WITH_EDITORONLY_DATA
	bRefreshEditorOnly = false;
#endif

	bAutoCreate = true;
	bOrientCamera = false;
	bBindToSelf = true;
	WorldWidgetTag = FGameplayTag();
	WorldWidgetClassOverride = nullptr;
	WidgetParam = FParameter();
	WidgetScale = FVector::OneVector;
	WidgetPoints = TMap<FName, USceneComponent*>();
	WorldWidget = nullptr;
}

void UWorldWidgetComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshParams();
	
	WidgetPoints.Add(GetFName(), this);
	
	TArray<USceneComponent*> ChildrenComps;
	GetChildrenComponents(false, ChildrenComps);
	for(auto Iter : ChildrenComps)
	{
		if(!WidgetPoints.Contains(Iter->GetFName()))
		{
			WidgetPoints.Add(Iter->GetFName(), Iter);
		}
	}

	if(bAutoCreate)
	{
		CreateWorldWidget(WidgetParam);
	}
}

void UWorldWidgetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyWorldWidget();

	Super::EndPlay(EndPlayReason);
}

void UWorldWidgetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bPlayerWorldWidgetsRequested) RefreshPlayerWorldWidgets();

	if(bOrientCamera)
	{
#if WITH_EDITOR
		if (!GetWorld() || !GetWorld()->IsGameWorld()) SetWorldRotation(FSceneManager::Get().GetActiveViewportViewRotation());
		else
#endif
		{
			UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
			if (Instance && Instance->GetLocalPlayers().Num() == 1)
			{
				ULocalPlayer* LocalPlayer = Instance->GetLocalPlayers()[0];
				APlayerController* Controller = LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
				if (Controller && Controller->PlayerCameraManager) SetWorldRotation(Controller->PlayerCameraManager->GetCameraRotation());
			}
		}
		SetRelativeScale3D(FVector(-WidgetScale.X, -WidgetScale.Y, WidgetScale.Z));
	}
	else
	{
		SetRelativeScale3D(WidgetScale);
	}

	if(GetWorld() && GetWorld()->IsGameWorld())
	{
		if(WorldWidget && GetWidgetSpace() == EWidgetSpace::World)
		{
			if(WorldWidget->Execute_IsTickAble(WorldWidget))
			{
				WorldWidget->Execute_OnTick(WorldWidget, DeltaTime);
			}
		
			if(WorldWidget->GetWidgetRefreshType() == EWidgetRefreshType::Tick)
			{
				WorldWidget->OnRefresh();
			}
		}
	}
}

void UWorldWidgetComponent::SetWidget(UUserWidget* InWidget)
{
	UWorldWidgetBase* PreviousWorldWidget = WorldWidget;
	if(PreviousWorldWidget && PreviousWorldWidget != InWidget)
	{
		WorldWidget = nullptr;
		PreviousWorldWidget->OnDestroy(EObjectDespawnMode::Destroy);
	}

	Super::SetWidget(InWidget);

	if(InWidget)
	{
		if(WorldWidget != InWidget)
		{
			WorldWidget = Cast<UWorldWidgetBase>(InWidget);
			if(WorldWidget)
			{
				const FParameter Param = MakeWidgetParam(this, WidgetParam);
				if(const FWorldWidgetConfig* Config = ResolveWorldWidgetConfig())
				{
					WorldWidget->WidgetTag = Config->ResolveWidgetTag();
				}
				IObjectPoolInterface::Execute_OnSpawn(WorldWidget, Param);
				WorldWidget->OnCreate(this, Param);
			}
		}
	}
}

void UWorldWidgetComponent::RefreshParams()
{
	const FWorldWidgetConfig* Config = ResolveWorldWidgetConfig();
	if(!Config)
	{
		return;
	}

	Space = Config->Space == EWorldWidgetSpace::World
		? EWidgetSpace::World
		: EWidgetSpace::Screen;
	bDrawAtDesiredSize = Config->bAutoSize;
	DrawSize = FIntPoint(Config->DrawSize.X, Config->DrawSize.Y);
	Pivot = Config->Alignment;
}

#if WITH_EDITOR
bool UWorldWidgetComponent::CanEditChange(const FProperty* InProperty) const
{
	if(InProperty)
	{
		const FString PropertyName = InProperty->GetName();

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, Space) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, WidgetClass) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, DrawSize) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, Pivot))
		{
			return false;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, bDrawAtDesiredSize) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, TickMode))
		{
			return Space == EWidgetSpace::World;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, CylinderArcAngle))
		{
			return Space == EWidgetSpace::World && GeometryMode == EWidgetGeometryMode::Cylinder;
		}
	}

	return Super::CanEditChange(InProperty);
}

void UWorldWidgetComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FProperty* Property = PropertyChangedEvent.MemberProperty;

	if(Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		static FName RefreshEditorOnlyName = GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, bRefreshEditorOnly);
		static FName SpaceName = GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, Space);
		static FName AutoCreateName = GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, bAutoCreate);
		static FName WorldWidgetTagName = GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, WorldWidgetTag);
		static FName WorldWidgetClassOverrideName = GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, WorldWidgetClassOverride);

		const FName PropertyName = Property->GetFName();

		if(PropertyName == RefreshEditorOnlyName)
		{
			if(bRefreshEditorOnly)
			{
				RefreshParams();
				bRefreshEditorOnly = false;
			}
		}

		if(PropertyName == WorldWidgetTagName || PropertyName == WorldWidgetClassOverrideName)
		{
			RefreshParams();
		}

		if(PropertyName == SpaceName || PropertyName == AutoCreateName || PropertyName == WorldWidgetTagName || PropertyName == WorldWidgetClassOverrideName)
		{
			if(Space == EWidgetSpace::World && bAutoCreate)
			{
				WidgetClass = ResolveWorldWidgetClass();
			}
			else
			{
				WidgetClass = nullptr;
			}
			// SetWorldWidgetClass(WidgetClass, true);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UWorldWidgetComponent::CreateWorldWidget(const FParameter& InParam, bool bInEditor)
{
	DestroyWorldWidget(EObjectDespawnMode::Destroy, bInEditor);
	RefreshParams();
	const FWorldWidgetConfig* Config = ResolveWorldWidgetConfig();
	const TSubclassOf<UWorldWidgetBase> ResolvedClass = ResolveWorldWidgetClass();
	const FParameter Param = MakeWidgetParam(
		this,
		InParam.HasValue() ? InParam : WidgetParam);
	if (!bInEditor && Space == EWidgetSpace::Screen)
	{
		ActiveWidgetParam = Param;
		bPlayerWorldWidgetsRequested = true;
		if (UWidgetModule* Widgets = UWidgetModule::GetPtr(GetWorld())) Widgets->RegisterWorldWidgetComponent(this);
		RefreshPlayerWorldWidgets();
		return;
	}
	
	if(Config && ResolvedClass)
	{
		switch(Space)
		{
			case EWidgetSpace::World:
			{
				UWorldWidgetBase* NewWorldWidget = CreateWidget<UWorldWidgetBase>(GetWorld(), ResolvedClass);
				Super::SetWidget(NewWorldWidget);
				WorldWidget = NewWorldWidget;
				if(WorldWidget)
				{
					WorldWidget->WidgetTag = Config->ResolveWidgetTag();
					IObjectPoolInterface::Execute_OnSpawn(WorldWidget, Param);
					WorldWidget->OnCreate(this, Param);
				}
				break;
			}
			case EWidgetSpace::Screen:
			{
				UWorldWidgetBase* _WorldWidget = nullptr;
				if(bInEditor)
				{
					_WorldWidget = CreateWidget<UWorldWidgetBase>(GetWorld(), ResolvedClass);
					if (_WorldWidget)
					{
						_WorldWidget->bWidgetInEditor = true;
						_WorldWidget->WidgetTag = Config->ResolveWidgetTag();
						IObjectPoolInterface::Execute_OnSpawn(_WorldWidget, Param);
						_WorldWidget->OnCreate(this, Param);
					}
				}
				else
				{
					UWidgetModule* Widgets = UWidgetModule::GetPtr(GetWorld());
					_WorldWidget = Widgets ? Widgets->CreateWorldWidgetByTag(
						Config->ResolveWidgetTag(),
						this,
						Param,
						ResolvedClass) : nullptr;
				}
				SetWorldWidget(_WorldWidget);
				break;
			}
		}
	}
}

void UWorldWidgetComponent::DestroyWorldWidget(EObjectDespawnMode InMode, bool bInEditor)
{
	if (UWidgetModule* Widgets = UWidgetModule::GetPtr(GetWorld())) Widgets->UnregisterWorldWidgetComponent(this);
	bPlayerWorldWidgetsRequested = false;
	ActiveWidgetParam.Reset();
	TArray<TObjectPtr<UWorldWidgetBase>> PlayerWidgets;
	PlayerWorldWidgets.GenerateValueArray(PlayerWidgets);
	PlayerWorldWidgets.Reset();
	if (UWidgetModule* Widgets = UWidgetModule::GetPtr(GetWorld()))
		for (UWorldWidgetBase* PlayerWidget : PlayerWidgets) if (PlayerWidget) Widgets->DestroyWorldWidget(PlayerWidget, InMode);
	if(UWorldWidgetBase* WidgetToDestroy = WorldWidget)
	{
		WorldWidget = nullptr;
		switch(Space)
		{
			case EWidgetSpace::World:
			{
				Super::SetWidget(nullptr);
				SetWidgetClass(nullptr);
				WidgetToDestroy->OnDestroy(InMode);
				break;
			}
			case EWidgetSpace::Screen:
			{
				if(bInEditor)
				{
					WidgetToDestroy->OnDestroy(InMode);
				}
				else
				{
					if (UWidgetModule* Widgets = UWidgetModule::GetPtr(GetWorld())) Widgets->DestroyWorldWidget(WidgetToDestroy, InMode);
				}
				break;
			}
		}
	}
}

void UWorldWidgetComponent::RefreshPlayerWorldWidgets()
{
	UWorld* World = GetWorld();
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	UWidgetModule* Widgets = World ? UWidgetModule::GetPtr(World) : nullptr;
	if (!Instance || !Widgets || !bPlayerWorldWidgetsRequested) return;
	Widgets->RegisterWorldWidgetComponent(this);
	const TArray<ULocalPlayer*> Players = Instance->GetLocalPlayers();
	TArray<UWorldWidgetBase*> Removed;
	for (auto It = PlayerWorldWidgets.CreateIterator(); It; ++It)
	{
		ULocalPlayer* LocalPlayer = It.Key();
		APlayerController* Controller = LocalPlayer ? LocalPlayer->GetPlayerController(World) : nullptr;
		if (!Players.Contains(LocalPlayer) || !Controller || !It.Value() || !It.Value()->GetWidgetTag().IsValid() || It.Value()->GetOwningPlayer() != Controller)
		{
			if (It.Value()) Removed.Add(It.Value());
			It.RemoveCurrent();
		}
	}
	for (UWorldWidgetBase* RemovedWidget : Removed) Widgets->DestroyWorldWidget(RemovedWidget);
	const FWorldWidgetConfig* Config = ResolveWorldWidgetConfig();
	const TSubclassOf<UWorldWidgetBase> ResolvedClass = ResolveWorldWidgetClass();
	if (!Config || !ResolvedClass) return;
	const FGameplayTag Tag = Config->ResolveWidgetTag();
	for (ULocalPlayer* LocalPlayer : Players)
	{
		APlayerController* Controller = LocalPlayer ? LocalPlayer->GetPlayerController(World) : nullptr;
		if (!Controller || PlayerWorldWidgets.Contains(LocalPlayer) || !Widgets->GetWorldWidgetContainer(Controller)) continue;
		FParameter Param = ActiveWidgetParam;
		FWidgetSpawnParameter* Spawn = Param.GetMutablePtr<FWidgetSpawnParameter>();
		if (!Spawn) continue;
		Spawn->OwningPlayer = Controller;
		UWorldWidgetBase* PlayerWidget = Widgets->CreateWorldWidgetByTag(Tag, this, Param, ResolvedClass);
		if (PlayerWidget)
			PlayerWorldWidgets.Add(LocalPlayer, PlayerWidget);
	}
}

void UWorldWidgetComponent::SetWorldWidget(UUserWidget* InWidget)
{
	switch(Space)
	{
		case EWidgetSpace::World:
		{
			SetWidget(InWidget);
			break;
		}
		case EWidgetSpace::Screen:
		{
			WorldWidget = Cast<UWorldWidgetBase>(InWidget);
			break;
		}
	}
}

void UWorldWidgetComponent::SetWorldWidgetClass(TSubclassOf<UWorldWidgetBase> InClass, bool bRefresh)
{
	if(WorldWidgetClassOverride == InClass) return;

	WorldWidgetClassOverride = InClass;
	if(UWidgetModule* Widgets = UWidgetModule::GetPtr(GetWorld()))
	{
		WorldWidgetTag = Widgets->ResolveWorldWidgetTagForClass(InClass, false);
	}

	RefreshParams();
	
	switch(Space)
	{
		case EWidgetSpace::World:
		{
			WidgetClass = ResolveWorldWidgetClass();
			if(bRefresh)
			{
				if(FSlateApplication::IsInitialized())
				{
					if(HasBegunPlay())
					{
						if(WidgetClass)
						{
							SetWidget(CreateWidget(GetWorld(), WidgetClass));
						}
						else
						{
							SetWidget(nullptr);
						}
					}
				}
			}
			break;
		}
		case EWidgetSpace::Screen:
		{
			if(bRefresh)
			{
				if(ResolveWorldWidgetClass())
				{
					CreateWorldWidget(WidgetParam);
				}
				else
				{
					DestroyWorldWidget();
				}
			}
			break;
		}
	}
}

void UWorldWidgetComponent::SetWorldWidgetTag(FGameplayTag InTag, bool bRefresh)
{
	if(WorldWidgetTag == InTag)
	{
		return;
	}

	WorldWidgetTag = InTag;
	RefreshParams();
	if(bRefresh)
	{
		CreateWorldWidget(WidgetParam);
	}
}

UUserWidget* UWorldWidgetComponent::GetWorldWidget(APlayerController* InPlayer) const
{
	if (Space == EWidgetSpace::World) return GetUserWidgetObject();
	ULocalPlayer* LocalPlayer = InPlayer ? InPlayer->GetLocalPlayer() : nullptr;
	return LocalPlayer ? PlayerWorldWidgets.FindRef(LocalPlayer) : nullptr;
}

TArray<UWorldWidgetBase*> UWorldWidgetComponent::GetWorldWidgets() const
{
	TArray<UWorldWidgetBase*> Result;
	if (WorldWidget) Result.Add(WorldWidget);
	if (Space == EWidgetSpace::Screen) for (const auto& Entry : PlayerWorldWidgets) if (Entry.Value) Result.Add(Entry.Value);
	return Result;
}

USceneComponent* UWorldWidgetComponent::GetWidgetPoint(FName InPointName) const
{
	if(WidgetPoints.Contains(InPointName))
	{
		return WidgetPoints[InPointName];
	}
	return nullptr;
}

bool UWorldWidgetComponent::EDC_AutoCreate() const
{
	return !GetOwner() || !GetOwner()->IsA<AWorldWidgetActor>();
}

const FWorldWidgetConfig* UWorldWidgetComponent::ResolveWorldWidgetConfig() const
{
	UWidgetModule* Widgets = UWidgetModule::GetPtr(GetWorld());
	if(!Widgets)
	{
		return nullptr;
	}

	FGameplayTag WidgetTag = WorldWidgetTag;
	if(!WidgetTag.IsValid() && WorldWidgetClassOverride)
	{
		WidgetTag = Widgets->ResolveWorldWidgetTagForClass(WorldWidgetClassOverride, false);
	}
	return WidgetTag.IsValid()
		? Widgets->GetWorldWidgetConfig(WidgetTag)
		: nullptr;
}

TSubclassOf<UWorldWidgetBase> UWorldWidgetComponent::ResolveWorldWidgetClass() const
{
	if(WorldWidgetClassOverride)
	{
		return WorldWidgetClassOverride;
	}
	const FWorldWidgetConfig* Config = ResolveWorldWidgetConfig();
	return Config ? Config->WidgetClass : nullptr;
}

