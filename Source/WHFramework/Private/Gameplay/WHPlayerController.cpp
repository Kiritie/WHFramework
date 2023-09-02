// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/WHPlayerController.h"

#include "Net/UnrealNetwork.h"
#include "Audio/AudioModuleNetworkComponent.h"
#include "Camera/CameraModule.h"
#include "Camera/CameraModuleNetworkComponent.h"
#include "Camera/Base/CameraPawnBase.h"
#include "Character/CharacterModuleNetworkComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleNetworkComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Global/GlobalBPLibrary.h"
#include "Input/InputModule.h"
#include "Input/InputModuleBPLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Main/MainModule.h"
#include "Math/MathBPLibrary.h"
#include "Media/MediaModuleNetworkComponent.h"
#include "Network/NetworkModuleNetworkComponent.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/ProcedureModuleNetworkComponent.h"

AWHPlayerController::AWHPlayerController()
{
	WidgetInteractionComp = CreateDefaultSubobject<UWidgetInteractionComponent>(FName("WidgetInteractionComp"));
	WidgetInteractionComp->SetupAttachment(RootComponent);

	ModuleNetCompMap = TMap<TSubclassOf<UModuleNetworkComponentBase>, UModuleNetworkComponentBase*>();
	
	AudioModuleNetComp = CreateDefaultSubobject<UAudioModuleNetworkComponent>(FName("AudioModuleNetComp"));
	ModuleNetCompMap.Add(UAudioModuleNetworkComponent::StaticClass(), AudioModuleNetComp);
	
	CameraModuleNetComp = CreateDefaultSubobject<UCameraModuleNetworkComponent>(FName("CameraModuleNetComp"));
	ModuleNetCompMap.Add(UCameraModuleNetworkComponent::StaticClass(), CameraModuleNetComp);
	
	CharacterModuleNetComp = CreateDefaultSubobject<UCharacterModuleNetworkComponent>(FName("CharacterModuleNetComp"));
	ModuleNetCompMap.Add(UCharacterModuleNetworkComponent::StaticClass(), CharacterModuleNetComp);
	
	EventModuleNetComp = CreateDefaultSubobject<UEventModuleNetworkComponent>(FName("EventModuleNetComp"));
	ModuleNetCompMap.Add(UEventModuleNetworkComponent::StaticClass(), EventModuleNetComp);
	
	MediaModuleNetComp = CreateDefaultSubobject<UMediaModuleNetworkComponent>(FName("MediaModuleNetComp"));
	ModuleNetCompMap.Add(UMediaModuleNetworkComponent::StaticClass(), MediaModuleNetComp);
	
	NetworkModuleNetComp = CreateDefaultSubobject<UNetworkModuleNetworkComponent>(FName("NetworkModuleNetComp"));
	ModuleNetCompMap.Add(UNetworkModuleNetworkComponent::StaticClass(), NetworkModuleNetComp);
	
	ProcedureModuleNetComp = CreateDefaultSubobject<UProcedureModuleNetworkComponent>(FName("ProcedureModuleNetComp"));
	ModuleNetCompMap.Add(UProcedureModuleNetworkComponent::StaticClass(), ProcedureModuleNetComp);
}

void AWHPlayerController::OnInitialize_Implementation()
{
	
}

void AWHPlayerController::OnPreparatory_Implementation(EPhase InPhase)
{
	
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
		return UKismetSystemLibrary::LineTraceSingle(this, rayStart, rayEnd, UGlobalBPLibrary::GetGameTraceType(InGameTraceChannel), false, ignoreActors, EDrawDebugTrace::None, OutHitResult, true);
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
		return UKismetSystemLibrary::LineTraceSingle(this, rayStart, rayEnd, UGlobalBPLibrary::GetGameTraceType(InGameTraceChannel), false, ignoreActors, EDrawDebugTrace::None, OutHitResult, true);
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
