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
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAbilityPickUpVoxel::Initialize(FAbilityItem InItem)
{
	Super::Initialize(InItem);

	BoxComponent->SetBoxExtent(Item.GetData<UVoxelData>().GetRange() * UVoxelModuleBPLibrary::GetWorldData().BlockSize * (1 / Item.GetData<UVoxelData>().GetRange().Z) * 0.2f);

	MeshComponent->Initialize(EVoxelMeshType::PickUp);
	MeshComponent->BuildVoxel(FVoxelItem(Item.ID));
	MeshComponent->CreateMesh(0, false);
}

void AAbilityPickUpVoxel::OnPickUp(IAbilityPickerInterface* InPicker)
{
	Super::OnPickUp(InPicker);
}

UMeshComponent* AAbilityPickUpVoxel::GetMeshComponent() const
{
	return MeshComponent;
}
