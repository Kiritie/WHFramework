// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CharacterModule.h"

#include "Camera/CameraModuleBPLibrary.h"
#include "Camera/Base/CameraActorBase.h"
#include "Character/CharacterModuleNetworkComponent.h"
#include "Character/Base/CharacterBase.h"
#include "Gameplay/WHPlayerController.h"
#include "Common/CommonBPLibrary.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"
		
IMPLEMENTATION_MODULE(ACharacterModule)

// Sets default values
ACharacterModule::ACharacterModule()
{
	ModuleName = FName("CharacterModule");

	ModuleNetworkComponent = UCharacterModuleNetworkComponent::StaticClass();

	Characters = TArray<ACharacterBase*>();
	CharacterMap = TMap<FName, ACharacterBase*>();
	DefaultCharacter = nullptr;
	DefaultInstantSwitch = false;
	DefaultResetCamera = true;
	CurrentCharacter = nullptr;
}

ACharacterModule::~ACharacterModule()
{
	TERMINATION_MODULE(ACharacterModule)
}

#if WITH_EDITOR
void ACharacterModule::OnGenerate()
{
	Super::OnGenerate();

}

void ACharacterModule::OnDestroy()
{
	Super::OnDestroy();

}
#endif

void ACharacterModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	for(auto Iter : Characters)
	{
		if(Iter && !CharacterMap.Contains(Iter->GetNameC()))
		{
			CharacterMap.Add(Iter->GetNameC(), Iter);
		}
	}
}

void ACharacterModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		if(DefaultCharacter)
		{
			SwitchCharacter(DefaultCharacter, DefaultResetCamera, DefaultInstantSwitch);
		}
	}
}

void ACharacterModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void ACharacterModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void ACharacterModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void ACharacterModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);
}

void ACharacterModule::AddCharacterToList(ACharacterBase* InCharacter)
{
	if(!Characters.Contains(InCharacter))
	{
		Characters.Add(InCharacter);
		if(!CharacterMap.Contains(InCharacter->GetNameC()))
		{
			CharacterMap.Add(InCharacter->GetNameC(), InCharacter);
		}
	}
}

void ACharacterModule::RemoveCharacterFromList(ACharacterBase* InCharacter)
{
	if(Characters.Contains(InCharacter))
	{
		Characters.Remove(InCharacter);
		if(CharacterMap.Contains(InCharacter->GetNameC()))
		{
			CharacterMap.Remove(InCharacter->GetNameC());
		}
	}
}

void ACharacterModule::SwitchCharacter(ACharacterBase* InCharacter, bool bResetCamera, bool bInstant)
{
	if(!CurrentCharacter || CurrentCharacter != InCharacter)
	{
		if(AWHPlayerController* PlayerController = UCommonBPLibrary::GetPlayerController<AWHPlayerController>())
		{
			if(InCharacter)
			{
				if(CurrentCharacter && CurrentCharacter->GetDefaultController())
				{
					CurrentCharacter->GetDefaultController()->Possess(CurrentCharacter);
				}
				CurrentCharacter = InCharacter;
				PlayerController->Possess(InCharacter);
				UCameraModuleBPLibrary::EndTrackTarget();
				UCameraModuleBPLibrary::StartTrackTarget(InCharacter, ETrackTargetMode::LocationAndRotationOnce, ETrackTargetSpace::Local, FVector(-1.f), InCharacter->GetCameraTraceOffset(), bResetCamera ? 0.f : -1.f, bResetCamera ? 0.f : -1.f, -1.f, true, bInstant);
			}
			else if(CurrentCharacter)
			{
				PlayerController->UnPossess();
				if(CurrentCharacter->GetDefaultController())
				{
					CurrentCharacter->GetDefaultController()->Possess(CurrentCharacter);
				}
				CurrentCharacter = nullptr;
			}
		}
	}
}

void ACharacterModule::SwitchCharacterByClass(TSubclassOf<ACharacterBase> InClass, bool bResetCamera, bool bInstant)
{
	SwitchCharacter(GetCharacterByClass(InClass), bResetCamera, bInstant);
}

void ACharacterModule::SwitchCharacterByName(FName InName, bool bResetCamera, bool bInstant)
{
	SwitchCharacter(GetCharacterByName(InName), bResetCamera, bInstant);
}

bool ACharacterModule::HasCharacterByClass(TSubclassOf<ACharacterBase> InClass) const
{
	if(!InClass) return nullptr;
	
	const FName CharacterName = InClass->GetDefaultObject<ACharacterBase>()->GetNameC();
	return HasCharacterByName(CharacterName);
}

bool ACharacterModule::HasCharacterByName(FName InName) const
{
	return CharacterMap.Contains(InName);
}

ACharacterBase* ACharacterModule::GetCurrentCharacter() const
{
	return CurrentCharacter;
}

ACharacterBase* ACharacterModule::GetCurrentCharacter(TSubclassOf<ACharacterBase> InClass) const
{
	return CurrentCharacter;
}

ACharacterBase* ACharacterModule::GetCharacterByClass(TSubclassOf<ACharacterBase> InClass) const
{
	if(!InClass) return nullptr;
	
	const FName CharacterName = InClass->GetDefaultObject<ACharacterBase>()->GetNameC();
	return GetCharacterByName(CharacterName);
}

ACharacterBase* ACharacterModule::GetCharacterByName(FName InName) const
{
	for (auto Iter : Characters)
	{
		if(Iter->GetNameC() == InName)
		{
			return Iter;
		}
	}
	return nullptr;
}

void ACharacterModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACharacterModule, Characters);
}
