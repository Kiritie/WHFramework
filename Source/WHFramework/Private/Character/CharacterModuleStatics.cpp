// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CharacterModuleStatics.h"

#include "Character/CharacterModule.h"

ACharacterBase* UCharacterModuleStatics::GetCurrentCharacter()
{
	return UCharacterModule::Get().GetCurrentCharacter();
}

ACharacterBase* UCharacterModuleStatics::GetCurrentCharacter(TSubclassOf<ACharacterBase> InClass)
{
	return UCharacterModule::Get().GetCurrentCharacter(InClass);
}

void UCharacterModuleStatics::SwitchCharacter(ACharacterBase* InCharacter, bool bResetCamera, bool bInstant)
{
	UCharacterModule::Get().SwitchCharacter(InCharacter, bResetCamera, bInstant);
}

void UCharacterModuleStatics::SwitchCharacterByClass(TSubclassOf<ACharacterBase> InClass, bool bResetCamera, bool bInstant)
{
	UCharacterModule::Get().SwitchCharacterByClass(InClass, bResetCamera, bInstant);
}

void UCharacterModuleStatics::SwitchCharacterByName(FName InName, bool bResetCamera, bool bInstant)
{
	UCharacterModule::Get().SwitchCharacterByName(InName, bResetCamera, bInstant);
}

bool UCharacterModuleStatics::HasCharacterByClass(TSubclassOf<ACharacterBase> InClass)
{
	return UCharacterModule::Get().HasCharacterByClass(InClass);
}

bool UCharacterModuleStatics::HasCharacterByName(FName InName)
{
	return UCharacterModule::Get().HasCharacterByName(InName);
}

ACharacterBase* UCharacterModuleStatics::GetCharacterByClass(TSubclassOf<ACharacterBase> InClass)
{
	return UCharacterModule::Get().GetCharacterByClass(InClass);
}

ACharacterBase* UCharacterModuleStatics::GetCharacterByName(FName InName)
{
	return UCharacterModule::Get().GetCharacterByName(InName);
}

void UCharacterModuleStatics::AddCharacterToList(ACharacterBase* InCharacter)
{
	UCharacterModule::Get().AddCharacterToList(InCharacter);
}

void UCharacterModuleStatics::RemoveCharacterFromList(ACharacterBase* InCharacter)
{
	UCharacterModule::Get().RemoveCharacterFromList(InCharacter);
}
