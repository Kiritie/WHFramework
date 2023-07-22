// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CharacterModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Character/CharacterModule.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

ACharacterBase* UCharacterModuleBPLibrary::GetCurrentCharacter()
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		return CharacterModule->GetCurrentCharacter();
	}
	return nullptr;
}

ACharacterBase* UCharacterModuleBPLibrary::GetCurrentCharacter(TSubclassOf<ACharacterBase> InCharacterClass)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		return CharacterModule->GetCurrentCharacter(InCharacterClass);
	}
	return nullptr;
}

void UCharacterModuleBPLibrary::SwitchCharacter(ACharacterBase* InCharacter, bool bResetCamera, bool bInstant)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		CharacterModule->SwitchCharacter(InCharacter, bResetCamera, bInstant);
	}
}

void UCharacterModuleBPLibrary::SwitchCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass, bool bResetCamera, bool bInstant)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		CharacterModule->SwitchCharacterByClass(InCharacterClass, bResetCamera, bInstant);
	}
}

void UCharacterModuleBPLibrary::SwitchCharacterByName(FName InCharacterName, bool bResetCamera, bool bInstant)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		CharacterModule->SwitchCharacterByName(InCharacterName, bResetCamera, bInstant);
	}
}

bool UCharacterModuleBPLibrary::HasCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		return CharacterModule->HasCharacterByClass(InCharacterClass);
	}
	return false;
}

bool UCharacterModuleBPLibrary::HasCharacterByName(FName InCharacterName)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		return CharacterModule->HasCharacterByName(InCharacterName);
	}
	return false;
}

ACharacterBase* UCharacterModuleBPLibrary::GetCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		return CharacterModule->GetCharacterByClass(InCharacterClass);
	}
	return nullptr;
}

ACharacterBase* UCharacterModuleBPLibrary::GetCharacterByName(FName InCharacterName)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		return CharacterModule->GetCharacterByName(InCharacterName);
	}
	return nullptr;
}

void UCharacterModuleBPLibrary::AddCharacterToList(ACharacterBase* InCharacter)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		CharacterModule->AddCharacterToList(InCharacter);
	}
}

void UCharacterModuleBPLibrary::RemoveCharacterFromList(ACharacterBase* InCharacter)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		CharacterModule->RemoveCharacterFromList(InCharacter);
	}
}
