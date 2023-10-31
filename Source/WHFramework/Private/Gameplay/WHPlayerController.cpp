// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/WHPlayerController.h"

#include "Camera/Base/CameraManagerBase.h"
#include "Components/WidgetInteractionComponent.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Common/CommonBPLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Procedure/ProcedureModule.h"

AWHPlayerController::AWHPlayerController()
{
	PlayerCameraManagerClass = ACameraManagerBase::StaticClass();

	WidgetInteractionComp = CreateDefaultSubobject<UWidgetInteractionComponent>(FName("WidgetInteractionComp"));
	WidgetInteractionComp->SetupAttachment(RootComponent);
}

void AWHPlayerController::OnInitialize_Implementation()
{
	
}

void AWHPlayerController::OnPreparatory_Implementation(EPhase InPhase)
{
	for(const auto Iter : AMainModule::Get()->GetAllModule())
	{
		if(Iter->ModuleNetworkComponent)
		{
			UModuleNetworkComponentBase* NetworkComponent = NewObject<UModuleNetworkComponentBase>(this, Iter->ModuleNetworkComponent, *FString::Printf(TEXT("%sNetworkComp"), *Iter->ModuleName.ToString()));
			NetworkComponent->RegisterComponent();
		}
	}
}

void AWHPlayerController::OnRefresh_Implementation(float DeltaSeconds)
{
	
}

void AWHPlayerController::OnTermination_Implementation(EPhase InPhase)
{
	
}

void AWHPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnInitialize(this);
		Execute_OnPreparatory(this, EPhase::None);
	}
}

void AWHPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnTermination(this, EPhase::None);
	}
}

void AWHPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	SetPlayerPawn(InPawn);
}

void AWHPlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}

void AWHPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
}

void AWHPlayerController::Tick(float DeltaSeconds) 
{
	Super::Tick(DeltaSeconds);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}

bool AWHPlayerController::RaycastSingleFromAimPoint(float InRayDistance, ECollisionChannel InGameTraceChannel, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult) const
{
	int32 ViewportSizeX, ViewportSizeY;
	FVector SightPos, RayDirection;
	GetViewportSize(ViewportSizeX, ViewportSizeY);
	if(DeprojectScreenPositionToWorld(ViewportSizeX * 0.5f, ViewportSizeY * 0.5f, SightPos, RayDirection))
	{
		const FVector rayStart = PlayerCameraManager->GetCameraLocation();
		const FVector rayEnd = rayStart + RayDirection * InRayDistance;
		TArray<AActor*> ignoreActors = InIgnoreActors;
		ignoreActors.AddUnique(GetPawn());
		ignoreActors.AddUnique(GetPlayerPawn());
		return UKismetSystemLibrary::LineTraceSingle(this, rayStart, rayEnd, UCommonBPLibrary::GetGameTraceType(InGameTraceChannel), false, ignoreActors, EDrawDebugTrace::None, OutHitResult, true);
	}
	return false;
}

bool AWHPlayerController::RaycastSingleFromMousePosition(float InRayDistance, ECollisionChannel InGameTraceChannel, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult) const
{
	float MousePosX, MousePosY;
	FVector SightPos, RayDirection;
	GetMousePosition(MousePosX, MousePosY);
	if(DeprojectScreenPositionToWorld(MousePosX, MousePosY, SightPos, RayDirection))
	{
		const FVector rayStart = PlayerCameraManager->GetCameraLocation();
		const FVector rayEnd = rayStart + RayDirection * InRayDistance;
		TArray<AActor*> ignoreActors = InIgnoreActors;
		ignoreActors.AddUnique(GetPawn());
		ignoreActors.AddUnique(GetPlayerPawn());
		return UKismetSystemLibrary::LineTraceSingle(this, rayStart, rayEnd, UCommonBPLibrary::GetGameTraceType(InGameTraceChannel), false, ignoreActors, EDrawDebugTrace::None, OutHitResult, true);
	}
	return false;
}

void AWHPlayerController::SetPlayerPawn(APawn* InPlayerPawn)
{
	if(InPlayerPawn && !InPlayerPawn->Implements<UWHPlayerInterface>()) return;

	if(PlayerPawn != InPlayerPawn)
	{
		PlayerPawn = InPlayerPawn;
		OnPlayerPawnChanged.Broadcast(PlayerPawn);
	}
}
