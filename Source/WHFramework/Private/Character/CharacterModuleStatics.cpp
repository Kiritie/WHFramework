// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CharacterModuleStatics.h"

#include "Character/CharacterModule.h"

ACharacterBase* UCharacterModuleStatics::GetCurrentCharacter()
{
	if(UCharacterModule* CharacterModule = UCharacterModule::Get())
	{
		return CharacterModule->GetCurrentCharacter();
	}
	return nullptr;
}

ACharacterBase* UCharacterModuleStatics::GetCurrentCharacter(TSubclassOf<ACharacterBase> InClass)
{
	if(UCharacterModule* CharacterModule = UCharacterModule::Get())
	{
		return CharacterModule->GetCurrentCharacter(InClass);
	}
	return nullptr;
}

void UCharacterModuleStatics::SwitchCharacter(ACharacterBase* InCharacter, bool bResetCamera, bool bInstant)
{
	if(UCharacterModule* CharacterModule = UCharacterModule::Get())
	{
		CharacterModule->SwitchCharacter(InCharacter, bResetCamera, bInstant);
	}
}

void UCharacterModuleStatics::SwitchCharacterByClass(TSubclassOf<ACharacterBase> InClass, bool bResetCamera, bool bInstant)
{
	if(UCharacterModule* CharacterModule = UCharacterModule::Get())
	{
		CharacterModule->SwitchCharacterByClass(InClass, bResetCamera, bInstant);
	}
}

void UCharacterModuleStatics::SwitchCharacterByName(FName InName, bool bResetCamera, bool bInstant)
{
	if(UCharacterModule* CharacterModule = UCharacterModule::Get())
	{
		CharacterModule->SwitchCharacterByName(InName, bResetCamera, bInstant);
	}
}

bool UCharacterModuleStatics::HasCharacterByClass(TSubclassOf<ACharacterBase> InClass)
{
	if(UCharacterModule* CharacterModule = UCharacterModule::Get())
	{
		return CharacterModule->HasCharacterByClass(InClass);
	}
	return false;
}

bool UCharacterModuleStatics::HasCharacterByName(FName InName)
{
	if(UCharacterModule* CharacterModule = UCharacterModule::Get())
	{
		return CharacterModule->HasCharacterByName(InName);
	}
	return false;
}

ACharacterBase* UCharacterModuleStatics::GetCharacterByClass(TSubclassOf<ACharacterBase> InClass)
{
	if(UCharacterModule* CharacterModule = UCharacterModule::Get())
	{
		return CharacterModule->GetCharacterByClass(InClass);
	}
	return nullptr;
}

ACharacterBase* UCharacterModuleStatics::GetCharacterByName(FName InName)
{
	if(UCharacterModule* CharacterModule = UCharacterModule::Get())
	{
		return CharacterModule->GetCharacterByName(InName);
	}
	return nullptr;
}

void UCharacterModuleStatics::AddCharacterToList(ACharacterBase* InCharacter)
{
	if(UCharacterModule* CharacterModule = UCharacterModule::Get())
	{
		CharacterModule->AddCharacterToList(InCharacter);
	}
}

void UCharacterModuleStatics::RemoveCharacterFromList(ACharacterBase* InCharacter)
{
	if(UCharacterModule* CharacterModule = UCharacterModule::Get())
	{
		CharacterModule->RemoveCharacterFromList(InCharacter);
	}
}
