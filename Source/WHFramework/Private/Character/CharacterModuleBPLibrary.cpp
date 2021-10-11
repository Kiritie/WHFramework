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

void UCharacterModuleBPLibrary::RemoveCharacterFromListByName(const FName InCharacterName)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->RemoveCharacterFromListByName(InCharacterName);
	}
}

TScriptInterface<ICharacterInterface> UCharacterModuleBPLibrary::GetCharacterByName(const FName InCharacterName)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->GetCharacterByName(InCharacterName);
	}
	return nullptr;
}

void UCharacterModuleBPLibrary::PlayCharacterSound(const FName InCharacterName, USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->PlayCharacterSound(InCharacterName, InSound, InVolume, bMulticast);
	}
}

void UCharacterModuleBPLibrary::StopCharacterSound(const FName InCharacterName)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->StopCharacterSound(InCharacterName);
	}
}

void UCharacterModuleBPLibrary::PlayCharacterMontage(const FName InCharacterName, UAnimMontage* InMontage, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->PlayCharacterMontage(InCharacterName, InMontage, bMulticast);
	}
}

void UCharacterModuleBPLibrary::PlayCharacterMontageByName(const FName InCharacterName, const FName InMontageName, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->PlayCharacterMontageByName(InCharacterName, InMontageName, bMulticast);
	}
}

void UCharacterModuleBPLibrary::StopCharacterMontage(const FName InCharacterName, UAnimMontage* InMontage, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->StopCharacterMontage(InCharacterName, InMontage, bMulticast);
	}
}

void UCharacterModuleBPLibrary::StopCharacterMontageByName(const FName InCharacterName, const FName InMontageName, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->StopCharacterMontageByName(InCharacterName, InMontageName, bMulticast);
	}
}

void UCharacterModuleBPLibrary::TeleportCharacterTo(const FName InCharacterName, FTransform InTransform, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->TeleportCharacterTo(InCharacterName, InTransform, bMulticast);
	}
}

void UCharacterModuleBPLibrary::AIMoveCharacterTo(const FName InCharacterName, FTransform InTransform, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->AIMoveCharacterTo(InCharacterName, InTransform, bMulticast);
	}
}

void UCharacterModuleBPLibrary::StopCharacterAIMove(const FName InCharacterName, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->StopCharacterAIMove(InCharacterName, bMulticast);
	}
}

void UCharacterModuleBPLibrary::RotationCharacterTowards(const FName InCharacterName, FRotator InRotation, float InDuration, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->RotationCharacterTowards(InCharacterName, InRotation, InDuration, bMulticast);
	}
}
