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
	MeshComponent->Initialize(EVoxelMeshNature::PickUp);
}

void AAbilityPickUpVoxel::Initialize(FAbilityItem InItem)
{
	Super::Initialize(InItem);

	const FVector range = Item.GetData<UVoxelData>().GetRange();
	BoxComponent->SetBoxExtent(range * UVoxelModuleBPLibrary::GetWorldData().BlockSize * 0.15f);

	MeshComponent->CreateVoxel(InItem);
}

void AAbilityPickUpVoxel::OnPickUp(IAbilityPickerInterface* InPicker)
{
	Super::OnPickUp(InPicker);
}

UMeshComponent* AAbilityPickUpVoxel::GetMeshComponent() const
{
	return MeshComponent;
}
