// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CharacterModule.h"

#include "Camera/CameraModuleStatics.h"
#include "Camera/Actor/CameraActorBase.h"
#include "Character/CharacterModuleNetworkComponent.h"
#include "Character/Base/CharacterBase.h"
#include "Gameplay/WHPlayerController.h"
#include "Common/CommonStatics.h"
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
		if(Iter && !CharacterMap.Contains(Iter->GetNameC()))
		{
			CharacterMap.Add(Iter->GetNameC(), Iter);
			
			USceneModuleStatics::AddSceneActor(Iter);
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
	static FCharacterModuleSaveData* SaveData;
	SaveData = new FCharacterModuleSaveData();

	SaveData->CurrentCharacter = CurrentCharacter;

	return SaveData;
}

FString UCharacterModule::GetModuleDebugMessage()
{
	return FString::Printf(TEXT("CurrentCharacter: %s"), CurrentCharacter ? *CurrentCharacter->GetNameC().ToString() : TEXT("None"));
}

void UCharacterModule::AddCharacterToList(ACharacterBase* InCharacter)
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

void UCharacterModule::RemoveCharacterFromList(ACharacterBase* InCharacter)
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

void UCharacterModule::SwitchCharacter(ACharacterBase* InCharacter, bool bResetCamera, bool bInstant)
{
	if(CurrentCharacter == InCharacter) return;

	AWHPlayerController* PlayerController = UCommonStatics::GetPlayerController<AWHPlayerController>();
	
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
		UCameraModuleStatics::EndTrackTarget();
		UCameraModuleStatics::StartTrackTarget(CurrentCharacter, ICameraTrackableInterface::Execute_GetCameraTrackMode(CurrentCharacter), bInstant ? ECameraViewMode::Instant : ECameraViewMode::Smooth, ECameraViewSpace::Local, FVector::ZeroVector, ICameraTrackableInterface::Execute_GetCameraOffset(CurrentCharacter) * CurrentCharacter->GetActorScale3D(), bResetCamera ? 0.f : -1.f, bResetCamera ? 0.f : -1.f, ICameraTrackableInterface::Execute_GetCameraDistance(CurrentCharacter));
	}
	else if(CurrentCharacter)
	{
		CurrentCharacter->OnUnSwitch();
		PlayerController->UnPossess();
		if(CurrentCharacter->GetDefaultController())
		{
			CurrentCharacter->GetDefaultController()->Possess(CurrentCharacter);
		}
		CurrentCharacter = nullptr;
		UCameraModuleStatics::EndTrackTarget();
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
	
	const FName CharacterName = InClass->GetDefaultObject<ACharacterBase>()->GetNameC();
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
	
	const FName CharacterName = InClass->GetDefaultObject<ACharacterBase>()->GetNameC();
	return GetCharacterByName(CharacterName);
}

ACharacterBase* UCharacterModule::GetCharacterByName(FName InName) const
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

void UCharacterModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCharacterModule, Characters);
}
