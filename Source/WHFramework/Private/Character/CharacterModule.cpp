// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CharacterModule.h"

#include "Camera/CameraModuleBPLibrary.h"
#include "Character/Base/CharacterBase.h"
#include "Gameplay/WHPlayerController.h"
#include "Global/GlobalBPLibrary.h"
#include "Net/UnrealNetwork.h"
		
IMPLEMENTATION_MODULE(ACharacterModule)

// Sets default values
ACharacterModule::ACharacterModule()
{
	ModuleName = FName("CharacterModule");

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

	if(InPhase == EPhase::Final && DefaultCharacter)
	{
		SwitchCharacter(DefaultCharacter, DefaultResetCamera, DefaultInstantSwitch);
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
		if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
		{
			if(InCharacter)
			{
				UCameraModuleBPLibrary::SwitchCamera(nullptr);
				PlayerController->Possess(InCharacter);
				UCameraModuleBPLibrary::SetCameraRotationAndDistance(bResetCamera ? InCharacter->GetActorRotation().Yaw : -1, -1, -1, bInstant);
				if(CurrentCharacter && CurrentCharacter->GetDefaultController())
				{
					CurrentCharacter->GetDefaultController()->Possess(CurrentCharacter);
				}
				CurrentCharacter = InCharacter;
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

void ACharacterModule::SwitchCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass, bool bResetCamera, bool bInstant)
{
	SwitchCharacter(GetCharacterByClass(InCharacterClass), bResetCamera, bInstant);
}

void ACharacterModule::SwitchCharacterByName(FName InCharacterName, bool bResetCamera, bool bInstant)
{
	SwitchCharacter(GetCharacterByName(InCharacterName), bResetCamera, bInstant);
}

bool ACharacterModule::HasCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass) const
{
	if(!InCharacterClass) return nullptr;
	
	const FName CharacterName = InCharacterClass->GetDefaultObject<ACharacterBase>()->GetNameC();
	return HasCharacterByName(CharacterName);
}

bool ACharacterModule::HasCharacterByName(FName InCharacterName) const
{
	return CharacterMap.Contains(InCharacterName);
}

ACharacterBase* ACharacterModule::GetCurrentCharacter() const
{
	return CurrentCharacter;
}

ACharacterBase* ACharacterModule::GetCurrentCharacter(TSubclassOf<ACharacterBase> InCharacterClass) const
{
	return CurrentCharacter;
}

ACharacterBase* ACharacterModule::GetCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass) const
{
	if(!InCharacterClass) return nullptr;
	
	const FName CharacterName = InCharacterClass->GetDefaultObject<ACharacterBase>()->GetNameC();
	return GetCharacterByName(CharacterName);
}

ACharacterBase* ACharacterModule::GetCharacterByName(FName InCharacterName) const
{
	for (auto Iter : Characters)
	{
		if(Iter->GetNameC() == InCharacterName)
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
