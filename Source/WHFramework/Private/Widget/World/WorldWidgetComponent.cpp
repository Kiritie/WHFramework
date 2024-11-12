// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/World/WorldWidgetComponent.h"

#include "Camera/CameraModuleStatics.h"
#include "Common/CommonStatics.h"
#include "Scene/SceneManager.h"
#include "Widget/WidgetModuleStatics.h"
#include "Widget/World/WorldWidgetActor.h"

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
	WidgetParams = TArray<FParameter>();
	WidgetScale = FVector::OneVector;
	WidgetPoints = TMap<FName, USceneComponent*>();
	WorldWidget = nullptr;
}

void UWorldWidgetComponent::BeginPlay()
{
	Super::BeginPlay();
	
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
		CreateWorldWidget(WidgetParams);
	}
}

void UWorldWidgetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(UCommonStatics::IsPlaying() && EndPlayReason == EEndPlayReason::Type::Destroyed)
	{
		DestroyWorldWidget();
	}
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

	if(UCommonStatics::IsPlaying())
	{
		if(WorldWidget && GetWidgetSpace() == EWidgetSpace::World)
		{
			WorldWidget->Execute_OnTick(WorldWidget, DeltaTime);
		
			if(WorldWidget->GetWidgetRefreshType() == EWidgetRefreshType::Tick)
			{
				WorldWidget->OnRefresh();
			}
		}
	}
}

void UWorldWidgetComponent::SetWidget(UUserWidget* InWidget)
{
	Super::SetWidget(InWidget);

	if(InWidget)
	{
		if(WorldWidget != InWidget)
		{
			WorldWidget = Cast<UWorldWidgetBase>(InWidget);
			if(WorldWidget)
			{
				WorldWidget->OnCreate(GetOwner(), this, WidgetParams);
			}
		}
	}
	else if(WorldWidget)
	{
		WorldWidget->OnDestroy(false);
		WorldWidget = nullptr;
	}
}

void UWorldWidgetComponent::RefreshParams()
{
	if(!WorldWidgetClass) return;

	if(const UWorldWidgetBase* DefaultObject = Cast<UWorldWidgetBase>(WorldWidgetClass->GetDefaultObject()))
	{
		Space = DefaultObject->GetWidgetSpace();
		DrawSize = FIntPoint(DefaultObject->GetWidgetDrawSize().X, DefaultObject->GetWidgetDrawSize().Y);
		Pivot = DefaultObject->GetWidgetAlignment();
		WidgetParams = DefaultObject->GetWidgetParams();
	}
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
		static FName WorldWidgetClassName = GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, WorldWidgetClass);

		const FName PropertyName = Property->GetFName();

		if(PropertyName == RefreshEditorOnlyName)
		{
			if(bRefreshEditorOnly)
			{
				RefreshParams();
				bRefreshEditorOnly = false;
			}
		}

		if(PropertyName == WorldWidgetClassName)
		{
			RefreshParams();
		}

		if(PropertyName == SpaceName || PropertyName == AutoCreateName || PropertyName == WorldWidgetClassName)
		{
			if(Space == EWidgetSpace::World && bAutoCreate)
			{
				WidgetClass = WorldWidgetClass;
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

void UWorldWidgetComponent::CreateWorldWidget(const TArray<FParameter>& InParams, bool bInEditor)
{
	DestroyWorldWidget(false, bInEditor);
	
	if(WorldWidgetClass)
	{
		switch(Space)
		{
			case EWidgetSpace::World:
			{
				SetWorldWidget(CreateWidget(GetWorld(), WidgetClass));
				break;
			}
			case EWidgetSpace::Screen:
			{
				SetWorldWidget(UWidgetModuleStatics::CreateWorldWidget<UWorldWidgetBase>(GetOwner(), this, InParams.IsEmpty() ? WidgetParams : InParams, bInEditor, WorldWidgetClass));
				break;
			}
		}
	}
}

void UWorldWidgetComponent::CreateWorldWidget(const TArray<FParameter>* InParams, bool bInEditor)
{
	CreateWorldWidget(InParams ? *InParams : TArray<FParameter>(), bInEditor);
}

void UWorldWidgetComponent::DestroyWorldWidget(bool bRecovery, bool bInEditor)
{
	if(WorldWidget)
	{
		switch(Space)
		{
			case EWidgetSpace::World:
			{
				SetWidgetClass(nullptr);
				break;
			}
			case EWidgetSpace::Screen:
			{
				UWidgetModuleStatics::DestroyWorldWidget(WorldWidget, bRecovery, bInEditor);
				WorldWidget = nullptr;
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

void UWorldWidgetComponent::SetWorldWidgetClass(TSubclassOf<UUserWidget> InClass, bool bRefresh)
{
	if(WorldWidgetClass == InClass) return;

	WorldWidgetClass = InClass;

	RefreshParams();
	
	switch(Space)
	{
		case EWidgetSpace::World:
		{
			WidgetClass = InClass;
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
				if(WorldWidgetClass)
				{
					CreateWorldWidget(WidgetParams);
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

