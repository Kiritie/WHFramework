// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CharacterModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Character/CharacterModule.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

ACharacterBase* UCharacterModuleBPLibrary::GetCurrentCharacter()
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->GetCurrentCharacter();
	}
	return nullptr;
}

ACharacterBase* UCharacterModuleBPLibrary::GetCurrentCharacter(TSubclassOf<ACharacterBase> InCharacterClass)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->GetCurrentCharacter(InCharacterClass);
	}
	return nullptr;
}

void UCharacterModuleBPLibrary::SwitchCharacter(ACharacterBase* InCharacter)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		CharacterModule->SwitchCharacter(InCharacter);
	}
}

void UCharacterModuleBPLibrary::SwitchCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		CharacterModule->SwitchCharacterByClass(InCharacterClass);
	}
}

void UCharacterModuleBPLibrary::SwitchCharacterByName(FName InCharacterName)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		CharacterModule->SwitchCharacterByName(InCharacterName);
	}
}

bool UCharacterModuleBPLibrary::HasCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->HasCharacterByClass(InCharacterClass);
	}
	return false;
}

bool UCharacterModuleBPLibrary::HasCharacterByName(FName InCharacterName)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->HasCharacterByName(InCharacterName);
	}
	return false;
}

ACharacterBase* UCharacterModuleBPLibrary::GetCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->GetCharacterByClass(InCharacterClass);
	}
	return nullptr;
}

ACharacterBase* UCharacterModuleBPLibrary::GetCharacterByName(FName InCharacterName)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		return CharacterModule->GetCharacterByName(InCharacterName);
	}
	return nullptr;
}

void UCharacterModuleBPLibrary::AddCharacterToList(ACharacterBase* InCharacter)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		CharacterModule->AddCharacterToList(InCharacter);
	}
}

void UCharacterModuleBPLibrary::RemoveCharacterFromList(ACharacterBase* InCharacter)
{
	if(ACharacterModule* CharacterModule = AMainModule::GetModuleByClass<ACharacterModule>())
	{
		CharacterModule->RemoveCharacterFromList(InCharacter);
	}
}
