// Fill out your copyright notice in the Description page of Project Settings.


#include "Pawn/PawnModule.h"
#include "Common/LifecycleTypes.h"
#include "Camera/CameraViewTypes.h"
#include "Camera/CameraFeatureTypes.h"

#include "Camera/CameraModule.h"
#include "Camera/Manager/CameraManagerBase.h"
#include "Camera/Mode/FollowCameraMode.h"
#include "Pawn/PawnModuleNetworkComponent.h"
#include "Pawn/Base/PawnBase.h"
#include "Gameplay/WHPlayerController.h"
#include "Common/CommonModuleStatics.h"
#include "Net/UnrealNetwork.h"
#include "Misc/ScopeExit.h"
		
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
}

UPawnModule::~UPawnModule()
{
}

#if WITH_EDITOR
void UPawnModule::OnGenerate()
{
	Super::OnGenerate();
}

void UPawnModule::OnDestroy()
{
	Super::OnDestroy();

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
	return FString::Printf(TEXT("Registered Pawns: %d"), Pawns.Num());
}

void UPawnModule::AddPawnToList(APawnBase* InPawn)
{
	if(::IsValid(InPawn) && InPawn->GetWorld() == GetWorld() && !Pawns.Contains(InPawn))
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
	if(::IsValid(InPawn) && Pawns.Contains(InPawn))
	{
		Pawns.Remove(InPawn);
		if(PawnMap.FindRef(InPawn->GetNameP()) == InPawn)
		{
			PawnMap.Remove(InPawn->GetNameP());
		}
	}
}

bool UPawnModule::SwitchPawn(APlayerController* InPlayer, APawnBase* InPawn, bool bResetCamera, bool bInstant)
{
	if (!::IsValid(InPlayer) || InPlayer->GetWorld() != GetWorld() || !InPlayer->HasAuthority() ||
		(InPawn && (!::IsValid(InPawn) || InPawn->GetWorld() != GetWorld()))) return false;
	const TWeakObjectPtr<APlayerController> SwitchingPlayer(InPlayer);
	if (SwitchingPlayers.Contains(SwitchingPlayer)) return false;
	SwitchingPlayers.Add(SwitchingPlayer);
	ON_SCOPE_EXIT { SwitchingPlayers.Remove(SwitchingPlayer); };
	APawn* PreviousPossessed = InPlayer->GetPawn();
	if (PreviousPossessed == InPawn) return true;
	if (InPawn)
	{
		if (const APlayerController* Existing = Cast<APlayerController>(InPawn->GetController()); Existing && Existing != InPlayer) return false;
		InPlayer->Possess(InPawn);
		if (InPlayer->GetPawn() != InPawn)
		{
			if (::IsValid(PreviousPossessed)) InPlayer->Possess(PreviousPossessed);
			return false;
		}
	}
	else
	{
		InPlayer->UnPossess();
	}
	if (APawnBase* Previous = Cast<APawnBase>(PreviousPossessed))
	{
		Previous->OnUnSwitch();
		if (!::IsValid(InPlayer) || InPlayer->GetWorld() != GetWorld() || InPlayer->GetPawn() != InPawn) return false;
		if (::IsValid(Previous))
		{
			if (AController* DefaultController = Previous->GetDefaultController(); ::IsValid(DefaultController) && DefaultController != InPlayer && !DefaultController->GetPawn())
				DefaultController->Possess(Previous);
		}
	}
	if (::IsValid(InPawn)) InPawn->OnSwitch();
	if (!::IsValid(InPlayer) || InPlayer->GetWorld() != GetWorld() || InPlayer->GetPawn() != InPawn) return false;
	if (UPawnModuleNetworkComponent* Network = InPlayer->FindComponentByClass<UPawnModuleNetworkComponent>())
		Network->NotifyPawnSwitchCommitted(InPawn, bResetCamera, bInstant);
	return true;
}

bool UPawnModule::SwitchToDefaultPawn(APlayerController* InPlayer)
{
	return DefaultPawn && SwitchPawn(InPlayer, DefaultPawn, DefaultResetCamera, DefaultInstantSwitch);
}

bool UPawnModule::SwitchPawnByClass(APlayerController* InPlayer, TSubclassOf<APawnBase> InClass, bool bResetCamera, bool bInstant)
{
	APawnBase* Pawn = GetPawnByClass(InClass);
	return Pawn && SwitchPawn(InPlayer, Pawn, bResetCamera, bInstant);
}

bool UPawnModule::SwitchPawnByName(APlayerController* InPlayer, FName InName, bool bResetCamera, bool bInstant)
{
	APawnBase* Pawn = GetPawnByName(InName);
	return Pawn && SwitchPawn(InPlayer, Pawn, bResetCamera, bInstant);
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

APawnBase* UPawnModule::GetCurrentPawn(APlayerController* InPlayer, TSubclassOf<APawnBase> InClass) const
{
	APawnBase* Pawn = InPlayer && InPlayer->GetWorld() == GetWorld() ? Cast<APawnBase>(InPlayer->GetPawn()) : nullptr;
	return Pawn && (!InClass || Pawn->IsA(InClass)) ? Pawn : nullptr;
}

APawnBase* UPawnModule::GetPawnByClass(TSubclassOf<APawnBase> InClass) const
{
	if(!InClass) return nullptr;
	
	const FName PawnName = InClass->GetDefaultObject<APawnBase>()->GetNameP();
	return GetPawnByName(PawnName);
}

APawnBase* UPawnModule::GetPawnByName(FName InName) const
{
	APawnBase* Pawn = PawnMap.FindRef(InName);
	return ::IsValid(Pawn) && Pawn->GetWorld() == GetWorld() ? Pawn : nullptr;
}

void UPawnModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPawnModule, Pawns);
}
