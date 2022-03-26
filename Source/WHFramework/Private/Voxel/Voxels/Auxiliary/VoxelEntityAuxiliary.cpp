// Fill out your copyright notice in the Description page of Project Settings.


#include "VoxelAuxiliary/VoxelEntityAuxiliary.h"
#include "Voxel/Voxel.h"
#include "Character/Player/DWPlayerCharacter.h"
#include "Components/BoxComponent.h"

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

	BoxComponent->SetBoxExtent(GetVoxelItem().GetVoxelData().GetFinalRange() * 0.5f);
}
