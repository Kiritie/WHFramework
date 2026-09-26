#include "Voxel/WorldObject/VoxelWorldObjectPresenter.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"

AVoxelWorldObjectPresenter::AVoxelWorldObjectPresenter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Mesh);
	Light->SetVisibility(false);
	Light->SetCastShadows(false);
}

void AVoxelWorldObjectPresenter::Initialize(UVoxelWorldObjectDefinition* InDefinition, const FVoxelWorldObjectInstance& InObject, double InBlockSize)
{
	Definition = InDefinition;
	Object = InObject;
	SetActorLocationAndRotation((FVector(Object.Anchor) + FVector(.5)) * InBlockSize, FRotator(0., Object.Yaw * 90., 0.));
	Mesh->SetStaticMesh(Definition->Mesh);
	Mesh->SetRelativeTransform(Definition->MeshTransform);
	Light->SetLightColor(Definition->LightColor);
	Light->SetIntensity(Definition->LightIntensity);
	Light->SetAttenuationRadius(Definition->LightRadius);
}

void AVoxelWorldObjectPresenter::RefreshState(const FVoxelWorldObjectInstance& InObject, bool bInActive, bool bInLightVisible)
{
	Object = InObject;
	Light->SetVisibility(bInActive && bInLightVisible && Definition->LightIntensity > 0.f);
	bLightInRange = bInLightVisible;
}
