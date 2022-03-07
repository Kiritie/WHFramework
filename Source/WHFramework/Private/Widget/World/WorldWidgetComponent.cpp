// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/World/WorldWidgetComponent.h"

#include "Kismet/KismetMathLibrary.h"
#include "Widget/WidgetModuleBPLibrary.h"

UWorldWidgetComponent::UWorldWidgetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	bWantsInitializeComponent = true;

	WidgetClass = nullptr;
	bAutoCreate = true;
	bOrientCamera = true;
	bBindToSelf = true;
	WidgetParams = TArray<FParameter>();
	WidgetPoints = TMap<FName, USceneComponent*>();
	Widget = nullptr;
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
		CreateWidget();
	}
}

void UWorldWidgetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	DestroyWidget();
}

void UWorldWidgetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if(bOrientCamera)
	{
		if(const AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(this))
		{
			SetWorldRotation(FRotator(GetComponentRotation().Pitch, UKismetMathLibrary::FindLookAtRotation(GetComponentLocation(), PlayerController->PlayerCameraManager->GetCameraLocation()).Yaw, GetComponentRotation().Roll));
		}
	}
}

void UWorldWidgetComponent::CreateWidget()
{
	if(!Widget)
	{
		Widget = UWidgetModuleBPLibrary::CreateWorldWidget<UWorldWidgetBase>(GetOwner(), FVector::ZeroVector, this, &WidgetParams, WidgetClass);
	}
}

void UWorldWidgetComponent::DestroyWidget()
{
	if(Widget)
	{
		Widget->Destroy();
		Widget = nullptr;
	}
}

USceneComponent* UWorldWidgetComponent::GetWidgetPoint(FName InPointName) const
{
	if(WidgetPoints.Contains(InPointName))
	{
		return WidgetPoints[InPointName];
	}
	return nullptr;
}

