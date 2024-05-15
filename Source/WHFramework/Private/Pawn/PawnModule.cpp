// Fill out your copyright notice in the Description page of Project Settings.


#include "Pawn/PawnModule.h"

#include "Camera/CameraModuleStatics.h"
#include "Camera/Actor/CameraActorBase.h"
#include "Pawn/PawnModuleNetworkComponent.h"
#include "Pawn/Base/PawnBase.h"
#include "Gameplay/WHPlayerController.h"
#include "Common/CommonStatics.h"
#include "Net/UnrealNetwork.h"
		
IMPLEMENTATION_MODULE(UPawnModule)

// Sets default values
UPawnModule::UPawnModule()
{
	ModuleName = FName("PawnModule");
	ModuleDisplayName = FText::FromString(TEXT("Pawn Module"));

	ModuleNetworkComponent = UPawnModuleNetworkComponent::StaticClass();

	Pawns = TArray<APawnBase*>();
	PawnMap = TMap<FName, APawnBase*>();
	DefaultPawn = nullptr;
	DefaultInstantSwitch = false;
	DefaultResetCamera = true;
	CurrentPawn = nullptr;
}

UPawnModule::~UPawnModule()
{
	TERMINATION_MODULE(UPawnModule)
}

#if WITH_EDITOR
void UPawnModule::OnGenerate()
{
	Super::OnGenerate();
}

void UPawnModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UPawnModule)
}
#endif

void UPawnModule::OnInitialize()
{
	Super::OnInitialize();

	for(auto Iter : Pawns)
	{
		if(Iter && !PawnMap.Contains(Iter->GetNameP()))
		{
			PawnMap.Add(Iter->GetNameP(), Iter);
		}
	}
}

void UPawnModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		if(DefaultPawn)
		{
			SwitchPawn(DefaultPawn, DefaultResetCamera, DefaultInstantSwitch);
		}
	}
}

void UPawnModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
}

void UPawnModule::OnPause()
{
	Super::OnPause();
}

void UPawnModule::OnUnPause()
{
	Super::OnUnPause();
}

void UPawnModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}

FString UPawnModule::GetModuleDebugMessage()
{
	return FString::Printf(TEXT("CurrentPawn: %s"), CurrentPawn ? *CurrentPawn->GetNameP().ToString() : TEXT("None"));
}

void UPawnModule::AddPawnToList(APawnBase* InPawn)
{
	if(!Pawns.Contains(InPawn))
	{
		Pawns.Add(InPawn);
		if(!PawnMap.Contains(InPawn->GetNameP()))
		{
			PawnMap.Add(InPawn->GetNameP(), InPawn);
		}
	}
}

void UPawnModule::RemovePawnFromList(APawnBase* InPawn)
{
	if(Pawns.Contains(InPawn))
	{
		Pawns.Remove(InPawn);
		if(PawnMap.Contains(InPawn->GetNameP()))
		{
			PawnMap.Remove(InPawn->GetNameP());
		}
	}
}

void UPawnModule::SwitchPawn(APawnBase* InPawn, bool bResetCamera, bool bInstant)
{
	if(CurrentPawn == InPawn) return;

	AWHPlayerController* PlayerController = UCommonStatics::GetPlayerController<AWHPlayerController>();
	
	if(InPawn)
	{
		if(CurrentPawn)
		{
			CurrentPawn->OnUnSwitch();
			if(CurrentPawn->GetDefaultController())
			{
				CurrentPawn->GetDefaultController()->Possess(CurrentPawn);
			}
		}
		CurrentPawn = InPawn;
		PlayerController->Possess(CurrentPawn);
		CurrentPawn->OnSwitch();
		UCameraModuleStatics::EndTrackTarget();
		UCameraModuleStatics::StartTrackTarget(CurrentPawn, ICameraTrackableInterface::Execute_GetCameraTrackMode(CurrentPawn), bInstant ? ECameraViewMode::Instant : ECameraViewMode::Smooth, ECameraViewSpace::Local, FVector::ZeroVector, ICameraTrackableInterface::Execute_GetCameraOffset(CurrentPawn) * CurrentPawn->GetActorScale3D(), bResetCamera ? 0.f : -1.f, bResetCamera ? 0.f : -1.f, ICameraTrackableInterface::Execute_GetCameraDistance(CurrentPawn));
	}
	else if(CurrentPawn)
	{
		CurrentPawn->OnUnSwitch();
		PlayerController->UnPossess();
		if(CurrentPawn->GetDefaultController())
		{
			CurrentPawn->GetDefaultController()->Possess(CurrentPawn);
		}
		CurrentPawn = nullptr;
		UCameraModuleStatics::EndTrackTarget();
	}
}

void UPawnModule::SwitchPawnByClass(TSubclassOf<APawnBase> InClass, bool bResetCamera, bool bInstant)
{
	SwitchPawn(GetPawnByClass(InClass), bResetCamera, bInstant);
}

void UPawnModule::SwitchPawnByName(FName InName, bool bResetCamera, bool bInstant)
{
	SwitchPawn(GetPawnByName(InName), bResetCamera, bInstant);
}

bool UPawnModule::HasPawnByClass(TSubclassOf<APawnBase> InClass) const
{
	if(!InClass) return false;
	
	const FName PawnName = InClass->GetDefaultObject<APawnBase>()->GetNameP();
	return HasPawnByName(PawnName);
}

bool UPawnModule::HasPawnByName(FName InName) const
{
	return PawnMap.Contains(InName);
}

APawnBase* UPawnModule::GetCurrentPawn() const
{
	return CurrentPawn;
}

APawnBase* UPawnModule::GetCurrentPawn(TSubclassOf<APawnBase> InClass) const
{
	return CurrentPawn;
}

APawnBase* UPawnModule::GetPawnByClass(TSubclassOf<APawnBase> InClass) const
{
	if(!InClass) return nullptr;
	
	const FName PawnName = InClass->GetDefaultObject<APawnBase>()->GetNameP();
	return GetPawnByName(PawnName);
}

APawnBase* UPawnModule::GetPawnByName(FName InName) const
{
	for (auto Iter : Pawns)
	{
		if(Iter->GetNameP() == InName)
		{
			return Iter;
		}
	}
	return nullptr;
}

void UPawnModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPawnModule, Pawns);
}
