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

void ACharacterModule::RemoveCharacterFromListByName(const FName InCharacterName)
{
	RemoveCharacterFromList(GetCharacterByName(InCharacterName));
}

TScriptInterface<ICharacterInterface> ACharacterModule::GetCharacterByName(const FName InCharacterName) const
{
	for (auto Iter : Characters)
	{
		if(Iter->Execute_GetCharacterName(Iter.GetObject()) == InCharacterName)
		{
			return Iter;
		}
	}
	return nullptr;
}

void ACharacterModule::PlayCharacterSound(const FName InCharacterName, USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->Execute_PlaySound(Character.GetObject(), InSound, InVolume, bMulticast);
	}
}

void ACharacterModule::StopCharacterSound(const FName InCharacterName)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->Execute_StopSound(Character.GetObject());
	}
}

void ACharacterModule::PlayCharacterMontage(const FName InCharacterName, UAnimMontage* InMontage, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->Execute_PlayMontage(Character.GetObject(), InMontage, bMulticast);
	}
}

void ACharacterModule::PlayCharacterMontageByName(const FName InCharacterName, const FName InMontageName, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->Execute_PlayMontageByName(Character.GetObject(), InMontageName, bMulticast);
	}
}

void ACharacterModule::StopCharacterMontage(const FName InCharacterName, UAnimMontage* InMontage, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->Execute_StopMontage(Character.GetObject(), InMontage, bMulticast);
	}
}

void ACharacterModule::StopCharacterMontageByName(const FName InCharacterName, const FName InMontageName, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->Execute_StopMontageByName(Character.GetObject(), InMontageName, bMulticast);
	}
}

void ACharacterModule::TeleportCharacterTo(const FName InCharacterName, FTransform InTransform, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->Execute_TeleportTo(Character.GetObject(), InTransform, bMulticast);
	}
}

void ACharacterModule::AIMoveCharacterTo(const FName InCharacterName, FTransform InTransform, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->Execute_AIMoveTo(Character.GetObject(), InTransform, bMulticast);
	}
}

void ACharacterModule::StopCharacterAIMove(const FName InCharacterName, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->Execute_StopAIMove(Character.GetObject(), bMulticast);
	}
}

void ACharacterModule::RotationCharacterTowards(const FName InCharacterName, FRotator InRotation, float InDuration, bool bMulticast)
{
	if(TScriptInterface<ICharacterInterface> Character = GetCharacterByName(InCharacterName))
	{
		Character->Execute_RotationTowards(Character.GetObject(), InRotation, InDuration, bMulticast);
	}
}

void ACharacterModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACharacterModule, Characters);
}
