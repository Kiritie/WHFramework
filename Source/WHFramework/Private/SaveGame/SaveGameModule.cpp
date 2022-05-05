// Fill out your copyright notice in the Description page of Project Settings.


#include "SaveGame/SaveGameModule.h"

#include "Net/UnrealNetwork.h"

// ParamSets default values
ASaveGameModule::ASaveGameModule()
{
	ModuleName = FName("SaveGameModule");

	UserIndex = 0;
}

#if WITH_EDITOR
void ASaveGameModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void ASaveGameModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void ASaveGameModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void ASaveGameModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
}

void ASaveGameModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void ASaveGameModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void ASaveGameModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

bool ASaveGameModule::K2_HasSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass,  int32 InSaveIndex) const
{
	return HasSaveGame<USaveGameBase>(InSaveIndex, InSaveGameClass);
}

USaveGameBase* ASaveGameModule::K2_GetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass) const
{
	return GetSaveGame<USaveGameBase>(InSaveGameClass);
}

USaveGameBase* ASaveGameModule::K2_CreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	return CreateSaveGame<USaveGameBase>(InSaveIndex, InSaveGameClass);
}

bool ASaveGameModule::K2_SaveSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, bool bRefresh)
{
	return SaveSaveGame<USaveGameBase>(bRefresh, InSaveGameClass);
}

USaveGameBase* ASaveGameModule::K2_LoadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	return LoadSaveGame<USaveGameBase>(InSaveIndex, InSaveGameClass);
}

bool ASaveGameModule::K2_UnloadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass)
{
	return UnloadSaveGame<USaveGameBase>(InSaveGameClass);
}

bool ASaveGameModule::K2_ResetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass)
{
	return ResetSaveGame<USaveGameBase>(InSaveGameClass);
}

bool ASaveGameModule::K2_RefreshSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass)
{
	return RefreshSaveGame<USaveGameBase>(InSaveGameClass);
}

bool ASaveGameModule::K2_DestroySaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	return DestroySaveGame<USaveGameBase>(InSaveIndex, InSaveGameClass);
}

FString ASaveGameModule::GetSaveSlotName(FName InSaveName, int32 InSaveIndex) const
{
	return InSaveIndex > 0 ? FString::Printf(TEXT("SaveGame_%s%d"), *InSaveName.ToString(), InSaveIndex) : FString::Printf(TEXT("SaveGame_%s"), *InSaveName.ToString());
}

void ASaveGameModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}
