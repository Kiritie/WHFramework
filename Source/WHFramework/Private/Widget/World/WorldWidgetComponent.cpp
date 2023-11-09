// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/World/WorldWidgetComponent.h"

#include "Camera/CameraModuleStatics.h"
#include "Common/CommonStatics.h"
#include "Widget/WidgetModuleStatics.h"

UWorldWidgetComponent::UWorldWidgetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	UPrimitiveComponent::SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bRefreshEditorOnly = false;
	bAutoCreate = true;
	bOrientCamera = true;
	bBindToSelf = true;
	WidgetParams = TArray<FParameter>();
	WidgetPoints = TMap<FName, USceneComponent*>();
	WorldWidget = nullptr;
}

void UWorldWidgetComponent::BeginPlay()
{
	Super::BeginPlay();
	
	InitTransform = GetRelativeTransform();

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
		CreateWorldWidget();
	}
}

void UWorldWidgetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(UCommonStatics::IsPlaying())
	{
		DestroyWorldWidget();
	}
}

void UWorldWidgetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if(!UCommonStatics::IsPlaying()) return;
	
	if(bOrientCamera)
	{
		SetWorldRotation(UCameraModuleStatics::GetCameraRotation(true));
		SetRelativeScale3D(FVector(-InitTransform.GetScale3D().X, -InitTransform.GetScale3D().Y, InitTransform.GetScale3D().Z));
	}
	else
	{
		SetRelativeRotation(InitTransform.GetRotation());
		SetRelativeScale3D(InitTransform.GetScale3D());
	}

	if(WorldWidget && GetWidgetSpace() == EWidgetSpace::World)
	{
		WorldWidget->Execute_OnTick(WorldWidget, DeltaTime);
		
		if(WorldWidget->GetWidgetRefreshType() == EWidgetRefreshType::Tick)
		{
			WorldWidget->OnRefresh();
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
		WorldWidget->OnDestroy();
		WorldWidget = nullptr;
	}
}

#if WITH_EDITOR
bool UWorldWidgetComponent::CanEditChange(const FProperty* InProperty) const
{
	if(InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, WidgetClass) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, DrawSize) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, Pivot))
		{
			return false;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWorldWidgetComponent, bBindToSelf))
		{
			return Space == EWidgetSpace::Screen;
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
	FProperty* Property = PropertyChangedEvent.MemberProperty;

	if(Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		static FName RefreshEditorOnly("bRefreshEditorOnly");
		static FName SpaceName("Space");
		static FName AutoCreateName("bAutoCreate");
		static FName WorldWidgetClassName("WorldWidgetClass");

		auto PropertyName = Property->GetFName();

		if(PropertyName == RefreshEditorOnly)
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
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UWorldWidgetComponent::RefreshParams()
{
	if(WorldWidgetClass)
	{
		if(UWorldWidgetBase* DefaultObject = Cast<UWorldWidgetBase>(WorldWidgetClass->GetDefaultObject()))
		{
			DrawSize = FIntPoint(DefaultObject->GetWidgetDrawSize().X, DefaultObject->GetWidgetDrawSize().Y);
			Pivot = DefaultObject->GetWidgetAlignment();
			if(WidgetParams.Num() != DefaultObject->GetWidgetParams().Num())
			{
				WidgetParams = DefaultObject->GetWidgetParams();
			}
		}
	}
}
#endif

void UWorldWidgetComponent::CreateWorldWidget()
{
	if(!WorldWidget && WorldWidgetClass)
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
				SetWorldWidget(UWidgetModuleStatics::CreateWorldWidget<UWorldWidgetBase>(GetOwner(), this, &WidgetParams, WorldWidgetClass));
				break;
			}
		}
	}
}

void UWorldWidgetComponent::DestroyWorldWidget(bool bRecovery)
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
				UWidgetModuleStatics::DestroyWorldWidget(WorldWidget, bRecovery);
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
	switch(Space)
	{
		case EWidgetSpace::World:
		{
			if(WidgetClass != InClass)
			{
				WidgetClass = InClass;
				if(WorldWidgetClass != InClass)
				{
					WorldWidgetClass = InClass;
				}
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
			}
			break;
		}
		case EWidgetSpace::Screen:
		{
			if(WorldWidgetClass != InClass)
			{
				WorldWidgetClass = InClass;
				if(bRefresh)
				{
					if(WorldWidgetClass)
					{
						CreateWorldWidget();
					}
					else
					{
						DestroyWorldWidget();
					}
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

