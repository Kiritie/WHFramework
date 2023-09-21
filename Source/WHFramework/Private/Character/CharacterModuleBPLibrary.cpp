// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CharacterModuleBPLibrary.h"

#include "Character/CharacterModule.h"

ACharacterBase* UCharacterModuleBPLibrary::GetCurrentCharacter()
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		return CharacterModule->GetCurrentCharacter();
	}
	return nullptr;
}

ACharacterBase* UCharacterModuleBPLibrary::GetCurrentCharacter(TSubclassOf<ACharacterBase> InClass)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		return CharacterModule->GetCurrentCharacter(InClass);
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

void UCharacterModuleBPLibrary::SwitchCharacterByClass(TSubclassOf<ACharacterBase> InClass, bool bResetCamera, bool bInstant)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		CharacterModule->SwitchCharacterByClass(InClass, bResetCamera, bInstant);
	}
}

void UCharacterModuleBPLibrary::SwitchCharacterByName(FName InName, bool bResetCamera, bool bInstant)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		CharacterModule->SwitchCharacterByName(InName, bResetCamera, bInstant);
	}
}

bool UCharacterModuleBPLibrary::HasCharacterByClass(TSubclassOf<ACharacterBase> InClass)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		return CharacterModule->HasCharacterByClass(InClass);
	}
	return false;
}

bool UCharacterModuleBPLibrary::HasCharacterByName(FName InName)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		return CharacterModule->HasCharacterByName(InName);
	}
	return false;
}

ACharacterBase* UCharacterModuleBPLibrary::GetCharacterByClass(TSubclassOf<ACharacterBase> InClass)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		return CharacterModule->GetCharacterByClass(InClass);
	}
	return nullptr;
}

ACharacterBase* UCharacterModuleBPLibrary::GetCharacterByName(FName InName)
{
	if(ACharacterModule* CharacterModule = ACharacterModule::Get())
	{
		return CharacterModule->GetCharacterByName(InName);
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
