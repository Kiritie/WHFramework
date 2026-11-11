// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CharacterModuleStatics.h"

#include "Character/CharacterModule.h"
#include "Character/Base/CharacterBase.h"
#include "GameFramework/PlayerController.h"

ACharacterBase* UCharacterModuleStatics::GetCurrentCharacter(APlayerController* InController)
{
    UCharacterModule* Module = InController ? UCharacterModule::GetPtr(InController->GetWorld()) : nullptr;
    return Module ? Module->GetCurrentCharacter(InController) : nullptr;
}

ACharacterBase* UCharacterModuleStatics::GetCurrentCharacter(APlayerController* InController, TSubclassOf<ACharacterBase> InClass)
{
    UCharacterModule* Module = InController ? UCharacterModule::GetPtr(InController->GetWorld()) : nullptr;
    return Module ? Module->GetCurrentCharacter(InController, InClass) : nullptr;
}

TArray<ACharacterBase*> UCharacterModuleStatics::GetAllCharacter(const UObject* WorldContextObject)
{
    UCharacterModule* Module = WorldContextObject ? UCharacterModule::GetPtr(WorldContextObject->GetWorld()) : nullptr;
    return Module ? Module->GetAllCharacter() : TArray<ACharacterBase*>();
}

void UCharacterModuleStatics::SwitchCharacter(APlayerController* InController, ACharacterBase* InCharacter, bool bResetCamera, bool bInstant)
{
    if (UCharacterModule* Module = InController ? UCharacterModule::GetPtr(InController->GetWorld()) : nullptr)
        Module->SwitchCharacter(InController, InCharacter, bResetCamera, bInstant);
}

void UCharacterModuleStatics::SwitchCharacterByClass(APlayerController* InController, TSubclassOf<ACharacterBase> InClass, bool bResetCamera, bool bInstant)
{
    if (UCharacterModule* Module = InController ? UCharacterModule::GetPtr(InController->GetWorld()) : nullptr)
        Module->SwitchCharacterByClass(InController, InClass, bResetCamera, bInstant);
}

void UCharacterModuleStatics::SwitchCharacterByName(APlayerController* InController, FName InName, bool bResetCamera, bool bInstant)
{
    if (UCharacterModule* Module = InController ? UCharacterModule::GetPtr(InController->GetWorld()) : nullptr)
        Module->SwitchCharacterByName(InController, InName, bResetCamera, bInstant);
}

bool UCharacterModuleStatics::HasCharacterByClass(const UObject* WorldContextObject, TSubclassOf<ACharacterBase> InClass)
{
    UCharacterModule* Module = WorldContextObject ? UCharacterModule::GetPtr(WorldContextObject->GetWorld()) : nullptr;
    return Module && Module->HasCharacterByClass(InClass);
}

bool UCharacterModuleStatics::HasCharacterByName(const UObject* WorldContextObject, FName InName)
{
    UCharacterModule* Module = WorldContextObject ? UCharacterModule::GetPtr(WorldContextObject->GetWorld()) : nullptr;
    return Module && Module->HasCharacterByName(InName);
}

ACharacterBase* UCharacterModuleStatics::GetCharacterByClass(const UObject* WorldContextObject, TSubclassOf<ACharacterBase> InClass)
{
    UCharacterModule* Module = WorldContextObject ? UCharacterModule::GetPtr(WorldContextObject->GetWorld()) : nullptr;
    return Module ? Module->GetCharacterByClass(InClass) : nullptr;
}

ACharacterBase* UCharacterModuleStatics::GetCharacterByName(const UObject* WorldContextObject, FName InName)
{
    UCharacterModule* Module = WorldContextObject ? UCharacterModule::GetPtr(WorldContextObject->GetWorld()) : nullptr;
    return Module ? Module->GetCharacterByName(InName) : nullptr;
}

void UCharacterModuleStatics::AddCharacterToList(ACharacterBase* InCharacter)
{
    if (UCharacterModule* Module = InCharacter ? UCharacterModule::GetPtr(InCharacter->GetWorld()) : nullptr) Module->AddCharacterToList(InCharacter);
}

void UCharacterModuleStatics::RemoveCharacterFromList(ACharacterBase* InCharacter)
{
    if (UCharacterModule* Module = InCharacter ? UCharacterModule::GetPtr(InCharacter->GetWorld()) : nullptr) Module->RemoveCharacterFromList(InCharacter);
}
