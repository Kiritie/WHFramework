// Fill out your copyright notice in the Description page of Project Settings.


#include "Pawn/PawnModule.h"

#include "Camera/CameraModule.h"
#include "Camera/Manager/CameraManagerBase.h"
#include "Camera/Mode/FollowCameraMode.h"
#include "Pawn/PawnModuleNetworkComponent.h"
#include "Pawn/Base/PawnBase.h"
#include "Gameplay/WHPlayerController.h"
#include "Common/CommonModuleStatics.h"
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

	AWHPlayerController* PlayerController = UCommonModuleStatics::GetPlayerController<AWHPlayerController>();
	
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
		if(ACameraManagerBase* CameraManager = UCameraModule::Get().GetCameraManager())
		{
			FCameraModeContext Context;
			Context.Target = CurrentPawn;
			Context.Transition = bInstant ? FCameraTransitionParams::Instant() : FCameraTransitionParams::Smooth();
			CameraManager->SetMode(UFollowCameraMode::StaticClass(), Context);
		}
	}
	else if(CurrentPawn)
	{
		APawnBase* PreviousPawn = CurrentPawn;
		CurrentPawn->OnUnSwitch();
		PlayerController->UnPossess();
		if(CurrentPawn->GetDefaultController())
		{
			CurrentPawn->GetDefaultController()->Possess(CurrentPawn);
		}
		if(ACameraManagerBase* CameraManager = UCameraModule::Get().GetCameraManager())
		{
			CameraManager->ClearModeTarget();
		}
		CurrentPawn = nullptr;
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
