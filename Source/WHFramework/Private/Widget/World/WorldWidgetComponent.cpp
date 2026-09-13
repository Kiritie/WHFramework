// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/World/WorldWidgetComponent.h"

#include "Camera/CameraModuleStatics.h"
#include "Common/CommonModuleStatics.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Scene/SceneManager.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleStatics.h"
#include "Widget/World/WorldWidgetActor.h"

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
	if(UCommonModuleStatics::IsPlaying() && EndPlayReason == EEndPlayReason::Type::Destroyed)
	{
		DestroyWorldWidget();
	}

	Super::EndPlay(EndPlayReason);
}

void UWorldWidgetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if(bOrientCamera)
	{
#if WITH_EDITOR
		SetWorldRotation(GIsPlaying ? UCameraModuleStatics::GetCameraRotation(true) : FSceneManager::Get().GetActiveViewportViewRotation());
#else
		SetWorldRotation(UCameraModuleStatics::GetCameraRotation(true));
#endif
		SetRelativeScale3D(FVector(-WidgetScale.X, -WidgetScale.Y, WidgetScale.Z));
	}
	else
	{
		SetRelativeScale3D(WidgetScale);
	}

	if(UCommonModuleStatics::IsPlaying())
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
				UWorldWidgetBase* _WorldWidget;
				if(bInEditor)
				{
					_WorldWidget = CreateWidget<UWorldWidgetBase>(GetWorld(), ResolvedClass);
					_WorldWidget->bWidgetInEditor = true;
					_WorldWidget->WidgetTag = Config->ResolveWidgetTag();
					IObjectPoolInterface::Execute_OnSpawn(_WorldWidget, Param);
					_WorldWidget->OnCreate(this, Param);
				}
				else
				{
					_WorldWidget = UWidgetModuleStatics::CreateWorldWidgetByTag(
						Config->ResolveWidgetTag(),
						this,
						Param,
						ResolvedClass);
				}
				SetWorldWidget(_WorldWidget);
				break;
			}
		}
	}
}

void UWorldWidgetComponent::DestroyWorldWidget(EObjectDespawnMode InMode, bool bInEditor)
{
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
					UWidgetModuleStatics::DestroyWorldWidget(WidgetToDestroy, InMode);
				}
				break;
			}
		}
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
	if(UWidgetModule::IsValid())
	{
		WorldWidgetTag = UWidgetModule::Get().ResolveWorldWidgetTagForClass(InClass, false);
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
					if(HasBegunPlay() && !GetWorld()->bIsTearingDown)
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

UUserWidget* UWorldWidgetComponent::GetWorldWidget() const
{
	return Space == EWidgetSpace::World ? GetUserWidgetObject() : WorldWidget;
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
	if(!UWidgetModule::IsValid())
	{
		return nullptr;
	}

	FGameplayTag WidgetTag = WorldWidgetTag;
	if(!WidgetTag.IsValid() && WorldWidgetClassOverride)
	{
		WidgetTag = UWidgetModule::Get().ResolveWorldWidgetTagForClass(WorldWidgetClassOverride, false);
	}
	return WidgetTag.IsValid()
		? UWidgetModule::Get().GetWorldWidgetConfig(WidgetTag)
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

