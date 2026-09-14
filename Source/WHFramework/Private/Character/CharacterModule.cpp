// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CharacterModule.h"

#include "Camera/CameraModule.h"
#include "Camera/Manager/CameraManagerBase.h"
#include "Camera/Mode/FollowCameraMode.h"
#include "Character/CharacterModuleNetworkComponent.h"
#include "Character/Base/CharacterBase.h"
#include "Gameplay/WHPlayerController.h"
#include "Common/CommonModuleStatics.h"
#include "Net/UnrealNetwork.h"
#include "SaveGame/Module/CharacterSaveGame.h"
#include "Scene/SceneModuleStatics.h"

IMPLEMENTATION_MODULE(UCharacterModule)

// Sets default values
UCharacterModule::UCharacterModule()
{
	ModuleName = FName("CharacterModule");
	ModuleDisplayName = FText::FromString(TEXT("Character Module"));

	ModuleSaveGame = UCharacterSaveGame::StaticClass();
	
	ModuleNetworkComponent = UCharacterModuleNetworkComponent::StaticClass();

	Characters = TArray<ACharacterBase*>();
	CharacterMap = TMap<FName, ACharacterBase*>();
	DefaultCharacter = nullptr;
	DefaultInstantSwitch = false;
	DefaultResetCamera = true;
	CurrentCharacter = nullptr;
}

UCharacterModule::~UCharacterModule()
{
	TERMINATION_MODULE(UCharacterModule)
}

#if WITH_EDITOR
void UCharacterModule::OnGenerate()
{
	Super::OnGenerate();
}

void UCharacterModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UCharacterModule)
}
#endif

void UCharacterModule::OnInitialize()
{
	Super::OnInitialize();

	for(auto Iter : Characters)
	{
		if(Iter && !CharacterMap.Contains(Iter->GetNameP()))
		{
			CharacterMap.Add(Iter->GetNameP(), Iter);
		}
	}
}

void UCharacterModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		if(!CurrentCharacter && DefaultCharacter)
		{
			SwitchCharacter(DefaultCharacter, DefaultResetCamera, DefaultInstantSwitch);
		}
	}
}

void UCharacterModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
}

void UCharacterModule::OnPause()
{
	Super::OnPause();
}

void UCharacterModule::OnUnPause()
{
	Super::OnUnPause();
}

void UCharacterModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}

void UCharacterModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FCharacterModuleSaveData>();

	if(SaveData.IsSaved())
	{
		if(SaveData.CurrentCharacter)
		{
			SwitchCharacter(SaveData.CurrentCharacter, DefaultResetCamera, DefaultInstantSwitch);
		}
	}
}

FSaveData* UCharacterModule::ToData()
{
	FCharacterModuleSaveData& SaveData = GetMutableSaveData<FCharacterModuleSaveData>();
	SaveData = FCharacterModuleSaveData();

	SaveData.CurrentCharacter = CurrentCharacter;

	return &SaveData;
}

FString UCharacterModule::GetModuleDebugMessage()
{
	return FString::Printf(TEXT("CurrentCharacter: %s"), CurrentCharacter ? *CurrentCharacter->GetNameP().ToString() : TEXT("None"));
}

void UCharacterModule::AddCharacterToList(ACharacterBase* InCharacter)
{
	if(!Characters.Contains(InCharacter))
	{
		Characters.Add(InCharacter);
		if(!CharacterMap.Contains(InCharacter->GetNameP()))
		{
			CharacterMap.Add(InCharacter->GetNameP(), InCharacter);
		}
	}
}

void UCharacterModule::RemoveCharacterFromList(ACharacterBase* InCharacter)
{
	if(Characters.Contains(InCharacter))
	{
		Characters.Remove(InCharacter);
		if(CharacterMap.Contains(InCharacter->GetNameP()))
		{
			CharacterMap.Remove(InCharacter->GetNameP());
		}
	}
}

void UCharacterModule::SwitchCharacter(ACharacterBase* InCharacter, bool bResetCamera, bool bInstant)
{
	if(CurrentCharacter == InCharacter) return;

	AWHPlayerController* PlayerController = UCommonModuleStatics::GetPlayerController<AWHPlayerController>();
	
	if(InCharacter)
	{
		if(CurrentCharacter)
		{
			CurrentCharacter->OnUnSwitch();
			if(CurrentCharacter->GetDefaultController())
			{
				CurrentCharacter->GetDefaultController()->Possess(CurrentCharacter);
			}
		}
		CurrentCharacter = InCharacter;
		PlayerController->Possess(CurrentCharacter);
		CurrentCharacter->OnSwitch();
		if(ACameraManagerBase* CameraManager = UCameraModule::Get().GetCameraManager())
		{
			FCameraModeContext Context;
			Context.Target = CurrentCharacter;
			Context.Transition = bInstant ? FCameraTransitionParams::Instant() : FCameraTransitionParams::Smooth();
			CameraManager->SetMode(UFollowCameraMode::StaticClass(), Context);
		}
	}
	else if(CurrentCharacter)
	{
		ACharacterBase* PreviousCharacter = CurrentCharacter;
		CurrentCharacter->OnUnSwitch();
		PlayerController->UnPossess();
		if(CurrentCharacter->GetDefaultController())
		{
			CurrentCharacter->GetDefaultController()->Possess(CurrentCharacter);
		}
		if(ACameraManagerBase* CameraManager = UCameraModule::Get().GetCameraManager())
		{
			CameraManager->ClearModeTarget();
		}
		CurrentCharacter = nullptr;
	}
}

void UCharacterModule::SwitchCharacterByClass(TSubclassOf<ACharacterBase> InClass, bool bResetCamera, bool bInstant)
{
	SwitchCharacter(GetCharacterByClass(InClass), bResetCamera, bInstant);
}

void UCharacterModule::SwitchCharacterByName(FName InName, bool bResetCamera, bool bInstant)
{
	SwitchCharacter(GetCharacterByName(InName), bResetCamera, bInstant);
}

bool UCharacterModule::HasCharacterByClass(TSubclassOf<ACharacterBase> InClass) const
{
	if(!InClass) return false;
	
	const FName CharacterName = InClass->GetDefaultObject<ACharacterBase>()->GetNameP();
	return HasCharacterByName(CharacterName);
}

bool UCharacterModule::HasCharacterByName(FName InName) const
{
	return CharacterMap.Contains(InName);
}

ACharacterBase* UCharacterModule::GetCurrentCharacter() const
{
	return CurrentCharacter;
}

ACharacterBase* UCharacterModule::GetCurrentCharacter(TSubclassOf<ACharacterBase> InClass) const
{
	return CurrentCharacter;
}

ACharacterBase* UCharacterModule::GetCharacterByClass(TSubclassOf<ACharacterBase> InClass) const
{
	if(!InClass) return nullptr;
	
	const FName CharacterName = InClass->GetDefaultObject<ACharacterBase>()->GetNameP();
	return GetCharacterByName(CharacterName);
}

ACharacterBase* UCharacterModule::GetCharacterByName(FName InName) const
{
	for (auto Iter : Characters)
	{
		if(Iter->GetNameP() == InName)
		{
			return Iter;
		}
	}
	return nullptr;
}

void UCharacterModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCharacterModule, Characters);
}
