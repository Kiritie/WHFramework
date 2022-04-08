// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelEntityAuxiliary.h"
#include "Components/BoxComponent.h"
#include "Voxel/Assets/VoxelData.h"

// Sets default values
AVoxelEntityAuxiliary::AVoxelEntityAuxiliary()
{
	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	BoxComponent->SetCollisionProfileName(TEXT("DW_EntityAuxiliary"));
	BoxComponent->SetBoxExtent(FVector(20, 20, 20));
	BoxComponent->OnComponentHit.AddDynamic(this, &AVoxelEntityAuxiliary::OnCollision);
	SetRootComponent(BoxComponent);
}

// Called when the game starts or when spawned
void AVoxelEntityAuxiliary::BeginPlay()
{
	Super::BeginPlay();

}

void AVoxelEntityAuxiliary::OnCollision(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{

}

void AVoxelEntityAuxiliary::Initialize(AVoxelChunk* InOwnerChunk, FIndex InVoxelIndex)
{
	Super::Initialize(InOwnerChunk, InVoxelIndex);

	BoxComponent->SetBoxExtent(GetVoxelItem().GetData<UVoxelData>()->GetFinalRange() * 0.5f);
}
