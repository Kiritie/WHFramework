// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CharacterModule.h"

#include "Net/UnrealNetwork.h"

// Sets default values
ACharacterModule::ACharacterModule()
{
	ModuleName = FName("CharacterModule");
}

#if WITH_EDITOR
void ACharacterModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();

}

void ACharacterModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();

}
#endif

void ACharacterModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void ACharacterModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
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

void ACharacterModule::AddCharacterToList(TScriptInterface<ICharacterInterface> InCharacter)
{
	if(!Characters.Contains(InCharacter))
	{
		Characters.Add(InCharacter);
	}
}

void ACharacterModule::RemoveCharacterFromList(TScriptInterface<ICharacterInterface> InCharacter)
{
	if(Characters.Contains(InCharacter))
	{
		Characters.Remove(InCharacter);
	}
}

void ACharacterModule::RemoveCharacterFromListByName(FName InCharacterName)
{
	RemoveCharacterFromList(GetCharacterByName(InCharacterName));
}

TScriptInterface<ICharacterInterface> ACharacterModule::GetCharacterByName(FName InCharacterName) const
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

void ACharacterModule::PlayCharacterSound(FName InCharacterName, USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->PlaySound(InSound, InVolume, bMulticast);
	}
}

void ACharacterModule::StopCharacterSound(FName InCharacterName)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->StopSound();
	}
}

void ACharacterModule::PlayCharacterMontage(FName InCharacterName, UAnimMontage* InMontage, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->PlayMontage(InMontage, bMulticast);
	}
}

void ACharacterModule::PlayCharacterMontageByName(FName InCharacterName, const FName InMontageName, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->PlayMontageByName(InMontageName, bMulticast);
	}
}

void ACharacterModule::StopCharacterMontage(FName InCharacterName, UAnimMontage* InMontage, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->StopMontage(InMontage, bMulticast);
	}
}

void ACharacterModule::StopCharacterMontageByName(FName InCharacterName, const FName InMontageName, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->StopMontageByName(InMontageName, bMulticast);
	}
}

void ACharacterModule::TeleportCharacterTo(FName InCharacterName, FTransform InTransform, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->TransformTowards(InTransform, bMulticast);
	}
}

void ACharacterModule::AIMoveCharacterTo(FName InCharacterName, FVector InLocation, float InStopDistance, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->AIMoveTo(InLocation, InStopDistance, bMulticast);
	}
}

void ACharacterModule::StopCharacterAIMove(FName InCharacterName, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->StopAIMove(bMulticast);
	}
}

void ACharacterModule::RotationCharacterTowards(FName InCharacterName, FRotator InRotation, float InDuration, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->RotationTowards(InRotation, InDuration, bMulticast);
	}
}

void ACharacterModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACharacterModule, Characters);
}
