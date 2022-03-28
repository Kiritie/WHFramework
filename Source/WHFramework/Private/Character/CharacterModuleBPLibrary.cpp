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

void UCharacterModuleBPLibrary::RemoveCharacterFromListByName(const FString& InCharacterName)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->RemoveCharacterFromListByName(InCharacterName);
	}
}

TScriptInterface<ICharacterInterface> UCharacterModuleBPLibrary::GetCharacterByName(const FString& InCharacterName)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->GetCharacterByName(InCharacterName);
	}
	return nullptr;
}

void UCharacterModuleBPLibrary::PlayCharacterSound(const FString& InCharacterName, USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->PlayCharacterSound(InCharacterName, InSound, InVolume, bMulticast);
	}
}

void UCharacterModuleBPLibrary::StopCharacterSound(const FString& InCharacterName)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->StopCharacterSound(InCharacterName);
	}
}

void UCharacterModuleBPLibrary::PlayCharacterMontage(const FString& InCharacterName, UAnimMontage* InMontage, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->PlayCharacterMontage(InCharacterName, InMontage, bMulticast);
	}
}

void UCharacterModuleBPLibrary::PlayCharacterMontageByName(const FString& InCharacterName, const FName InMontageName, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->PlayCharacterMontageByName(InCharacterName, InMontageName, bMulticast);
	}
}

void UCharacterModuleBPLibrary::StopCharacterMontage(const FString& InCharacterName, UAnimMontage* InMontage, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->StopCharacterMontage(InCharacterName, InMontage, bMulticast);
	}
}

void UCharacterModuleBPLibrary::StopCharacterMontageByName(const FString& InCharacterName, const FName InMontageName, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->StopCharacterMontageByName(InCharacterName, InMontageName, bMulticast);
	}
}

void UCharacterModuleBPLibrary::TeleportCharacterTo(const FString& InCharacterName, FTransform InTransform, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->TeleportCharacterTo(InCharacterName, InTransform, bMulticast);
	}
}

void UCharacterModuleBPLibrary::AIMoveCharacterTo(const FString& InCharacterName, FVector InLocation, float InStopDistance, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->AIMoveCharacterTo(InCharacterName, InLocation, InStopDistance, bMulticast);
	}
}

void UCharacterModuleBPLibrary::StopCharacterAIMove(const FString& InCharacterName, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->StopCharacterAIMove(InCharacterName, bMulticast);
	}
}

void UCharacterModuleBPLibrary::RotationCharacterTowards(const FString& InCharacterName, FRotator InRotation, float InDuration, bool bMulticast)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->RotationCharacterTowards(InCharacterName, InRotation, InDuration, bMulticast);
	}
}
