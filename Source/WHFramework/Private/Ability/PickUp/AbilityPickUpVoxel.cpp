// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpVoxel.h"

#include "Components/BoxComponent.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Datas/VoxelData.h"

AAbilityPickUpVoxel::AAbilityPickUpVoxel()
{
	MeshComponent = CreateDefaultSubobject<UVoxelMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetRelativeScale3D(FVector(0.3f));
	MeshComponent->Initialize(EVoxelMeshNature::PickUp);
}

void AAbilityPickUpVoxel::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);

	const FVector Range = Item.GetData<UVoxelData>().GetRange();
	BoxComponent->SetBoxExtent(Range * AVoxelModule::Get()->GetWorldData().BlockSize * 0.15f);

	MeshComponent->CreateVoxel(Item);
}

void AAbilityPickUpVoxel::OnPickUp_Implementation(const TScriptInterface<IAbilityPickerInterface>& InPicker)
{
	Super::OnPickUp_Implementation(InPicker);
}

UMeshComponent* AAbilityPickUpVoxel::GetMeshComponent() const
{
	return MeshComponent;
}
