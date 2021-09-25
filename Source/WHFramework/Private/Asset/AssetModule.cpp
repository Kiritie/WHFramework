// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModule.h"

// Sets default values
AAssetModule::AAssetModule()
{
	ModuleName = FName("AssetModule");

}

#if WITH_EDITOR
void AAssetModule::OnGenerate_Implementation()
{
	
}

void AAssetModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AAssetModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AAssetModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AAssetModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AAssetModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}
