// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CharacterModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Character/CharacterModule.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

void UCharacterModuleBPLibrary::AddCharacterToList(TScriptInterface<ICharacterInterface> InCharacter)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->AddCharacterToList(InCharacter);
	}
}

void UCharacterModuleBPLibrary::RemoveCharacterFromList(TScriptInterface<ICharacterInterface> InCharacter)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->RemoveCharacterFromList(InCharacter);
	}
}

void UCharacterModuleBPLibrary::RemoveCharacterFromListByName(FName InCharacterName)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->RemoveCharacterFromListByName(InCharacterName);
	}
}

TScriptInterface<ICharacterInterface> UCharacterModuleBPLibrary::GetCharacterByName(FName InCharacterName)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->GetCharacterByName(InCharacterName);
	}
	return nullptr;
}

void UCharacterModuleBPLibrary::PlayCharacterSound(FName InCharacterName, USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->PlayCharacterSound(InCharacterName, InSound, InVolume, bMulticast);
	}
}

void UCharacterModuleBPLibrary::StopCharacterSound(FName InCharacterName)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->StopCharacterSound(InCharacterName);
	}
}

void UCharacterModuleBPLibrary::PlayCharacterMontage(FName InCharacterName, UAnimMontage* InMontage, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->PlayCharacterMontage(InCharacterName, InMontage, bMulticast);
	}
}

void UCharacterModuleBPLibrary::PlayCharacterMontageByName(FName InCharacterName, const FName InMontageName, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->PlayCharacterMontageByName(InCharacterName, InMontageName, bMulticast);
	}
}

void UCharacterModuleBPLibrary::StopCharacterMontage(FName InCharacterName, UAnimMontage* InMontage, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->StopCharacterMontage(InCharacterName, InMontage, bMulticast);
	}
}

void UCharacterModuleBPLibrary::StopCharacterMontageByName(FName InCharacterName, const FName InMontageName, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->StopCharacterMontageByName(InCharacterName, InMontageName, bMulticast);
	}
}

void UCharacterModuleBPLibrary::TeleportCharacterTo(FName InCharacterName, FTransform InTransform, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->TeleportCharacterTo(InCharacterName, InTransform, bMulticast);
	}
}

void UCharacterModuleBPLibrary::AIMoveCharacterTo(FName InCharacterName, FVector InLocation, float InStopDistance, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->AIMoveCharacterTo(InCharacterName, InLocation, InStopDistance, bMulticast);
	}
}

void UCharacterModuleBPLibrary::StopCharacterAIMove(FName InCharacterName, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->StopCharacterAIMove(InCharacterName, bMulticast);
	}
}

void UCharacterModuleBPLibrary::RotationCharacterTowards(FName InCharacterName, FRotator InRotation, float InDuration, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->RotationCharacterTowards(InCharacterName, InRotation, InDuration, bMulticast);
	}
}
