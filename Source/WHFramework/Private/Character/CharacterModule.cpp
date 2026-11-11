// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CharacterModule.h"
#include "WHFrameworkCoreMacros.h"
#include "Common/LifecycleTypes.h"
#include "Camera/CameraViewTypes.h"
#include "Camera/CameraFeatureTypes.h"

#include "Camera/CameraModule.h"
#include "Camera/Manager/CameraManagerBase.h"
#include "Camera/Mode/FollowCameraMode.h"
#include "Character/CharacterModuleNetworkComponent.h"
#include "Character/Base/CharacterBase.h"
#include "Gameplay/WHPlayerController.h"
#include "Common/CommonModuleStatics.h"
#include "Net/UnrealNetwork.h"
#include "Scene/SceneModuleStatics.h"
#include "Engine/World.h"

IMPLEMENTATION_MODULE(UCharacterModule)

// Sets default values
UCharacterModule::UCharacterModule()
{
	ModuleName = FName("CharacterModule");
	ModuleDisplayName = FText::FromString(TEXT("Character Module"));
	SaveScope = ESaveScope::World;


	ModuleNetworkComponent = UCharacterModuleNetworkComponent::StaticClass();

	Characters = TArray<ACharacterBase*>();
	CharacterMap = TMap<FName, ACharacterBase*>();
}

UCharacterModule::~UCharacterModule()
{
}

#if WITH_EDITOR
void UCharacterModule::OnGenerate()
{
	Super::OnGenerate();
}

void UCharacterModule::OnDestroy()
{
	Super::OnDestroy();

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

void UCharacterModule::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	bCharacterLoadSucceeded = false;
	const FCharacterModuleSaveData* SaveData = InSaveData.GetPtr<FCharacterModuleSaveData>();
	if (!SaveData) return;
	TSet<FGuid> RecordIds;
	for (const FCharacterSaveRecord& Record : SaveData->Characters)
	{
		if (!Record.CharacterId.IsValid() || RecordIds.Contains(Record.CharacterId)) return;
		RecordIds.Add(Record.CharacterId);
	}
	for (const EPhase Phase : { EPhase::Primary, EPhase::Final })
	{
		if (!PHASEC(InPhase, Phase)) continue;
		for (const FCharacterSaveRecord& Record : SaveData->Characters)
		{
			ACharacterBase* const* Entry = Characters.FindByPredicate([this, &Record](const ACharacterBase* Candidate)
			{
				return ::IsValid(Candidate) && ShouldSaveCharacter(Candidate) && Candidate->GetActorID_Implementation() == Record.CharacterId;
			});
			ACharacterBase* Character = Entry ? *Entry : nullptr;
			if (!Character)
			{
				UE_LOG(LogTemp, Error, TEXT("角色存档缺少原世界角色：%s"), *Record.CharacterId.ToString());
				return;
			}
			if (!Character->LoadSaveData(Record.Data, Phase)) return;
		}
	}
	bCharacterLoadSucceeded = true;
}

FParameter UCharacterModule::ToData()
{
	FCharacterModuleSaveData SaveData;
	const TArray<ACharacterBase*> CapturedCharacters = Characters;
	for (ACharacterBase* Character : CapturedCharacters)
	{
		if (Character && !ShouldSaveCharacter(Character)) continue;
		if (!::IsValid(Character) || Character->GetWorld() != GetWorld())
		{
			UE_LOG(LogTemp, Error, TEXT("Character module capture lost a registered character"));
			return FParameter();
		}
		FCharacterSaveRecord Record;
		Record.CharacterId = Character->GetActorID_Implementation();
		Record.CharacterAssetId = Character->GetAssetID_Implementation();
		Record.Data = Character->GetSaveData(true);
		if (!Record.Data.HasValue())
		{
			UE_LOG(LogTemp, Error, TEXT("Character module capture failed for %s; no partial module will be saved"),
				*Record.CharacterId.ToString());
			return FParameter();
		}
		SaveData.Characters.Add(MoveTemp(Record));
	}
	return FParameter(MoveTemp(SaveData));
}

bool UCharacterModule::ShouldSaveCharacter(const ACharacterBase* InCharacter) const
{
	return true;
}

FString UCharacterModule::GetModuleDebugMessage()
{
	return FString::Printf(TEXT("Characters: %d"), Characters.Num());
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

void UCharacterModule::SwitchCharacter(APlayerController* InController, ACharacterBase* InCharacter, bool bResetCamera, bool bInstant)
{
    if (!::IsValid(InController) || InController->GetWorld() != GetWorld() ||
        (InCharacter && (!::IsValid(InCharacter) || InCharacter->GetWorld() != GetWorld()))) return;
    ACharacterBase* Previous = Cast<ACharacterBase>(InController->GetPawn());
    if (Previous == InCharacter) return;
    if (!InController->HasAuthority())
    {
        if (AWHPlayerController* Player = Cast<AWHPlayerController>(InController); Player && Player->IsLocalController())
            Player->ServerRequestCharacterSwitch(InCharacter, bResetCamera, bInstant);
        return;
    }
    if (InCharacter && Cast<APlayerController>(InCharacter->GetController()) && InCharacter->GetController() != InController) return;
    if (Previous)
    {
        Previous->OnUnSwitch();
        InController->UnPossess();
        if (AController* DefaultController = Previous->GetDefaultController()) DefaultController->Possess(Previous);
    }
    if (InCharacter)
    {
        InController->Possess(InCharacter);
        if (InController->GetPawn() != InCharacter) return;
        InCharacter->OnSwitch();
    }
    if (bResetCamera && !InController->IsLocalController())
    {
        if (AWHPlayerController* Player = Cast<AWHPlayerController>(InController)) Player->ClientApplyCharacterView(InCharacter, bInstant);
    }
    else if (bResetCamera)
    {
        if (ACameraManagerBase* CameraManager = Cast<ACameraManagerBase>(InController->PlayerCameraManager))
        {
            if (InCharacter)
            {
                FCameraModeContext Context;
                Context.Target = InCharacter;
                Context.Transition = bInstant ? FCameraTransitionParams::Instant() : FCameraTransitionParams::Smooth();
                CameraManager->SetMode(UFollowCameraMode::StaticClass(), Context);
            }
            else CameraManager->ClearModeTarget();
        }
    }
}

void UCharacterModule::SwitchCharacterByClass(APlayerController* InController, TSubclassOf<ACharacterBase> InClass, bool bResetCamera, bool bInstant)
{
    SwitchCharacter(InController, GetCharacterByClass(InClass), bResetCamera, bInstant);
}

void UCharacterModule::SwitchCharacterByName(APlayerController* InController, FName InName, bool bResetCamera, bool bInstant)
{
    SwitchCharacter(InController, GetCharacterByName(InName), bResetCamera, bInstant);
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

ACharacterBase* UCharacterModule::GetCurrentCharacter(APlayerController* InController) const
{
    return ::IsValid(InController) && InController->GetWorld() == GetWorld() ? Cast<ACharacterBase>(InController->GetPawn()) : nullptr;
}

ACharacterBase* UCharacterModule::GetCurrentCharacter(APlayerController* InController, TSubclassOf<ACharacterBase> InClass) const
{
    ACharacterBase* Character = GetCurrentCharacter(InController);
    return Character && (!InClass || Character->IsA(InClass)) ? Character : nullptr;
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
