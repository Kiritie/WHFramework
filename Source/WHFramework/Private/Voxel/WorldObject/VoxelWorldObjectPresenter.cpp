#include "Voxel/WorldObject/VoxelWorldObjectPresenter.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AVoxelWorldObjectPresenter::AVoxelWorldObjectPresenter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
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
	if (GetNetMode() != NM_DedicatedServer)
	{
		Mesh->SetStaticMesh(Definition->Mesh.LoadSynchronous());
		if (Definition->LightIntensity > 0.f)
		{
			for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
			{
				if (UMaterialInstanceDynamic* Material = Mesh->CreateDynamicMaterialInstance(Index)) EmissiveMaterials.Add(Material);
			}
		}
		for (const FVoxelWorldObjectVisualPart& Part : Definition->VisualParts)
		{
			UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
			Component->SetupAttachment(RootComponent);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetStaticMesh(Part.Mesh.LoadSynchronous());
			Component->SetRelativeLocation(Part.ClosedOffset);
			Component->RegisterComponent();
			VisualMeshes.Add(Component);
		}
	}
	Mesh->SetRelativeTransform(Definition->MeshTransform);
	Light->SetLightColor(Definition->LightColor);
	Light->SetIntensity(Definition->LightIntensity);
	Light->SetAttenuationRadius(Definition->LightRadius);
}

void AVoxelWorldObjectPresenter::RefreshState(const FVoxelWorldObjectInstance& InObject, bool bInActive, bool bInLightVisible)
{
	Object = InObject;
	const bool bLightVisible = GetNetMode() != NM_DedicatedServer && bInActive && bInLightVisible && Definition->LightIntensity > 0.f;
	Light->SetVisibility(bLightVisible);
	for (UMaterialInstanceDynamic* Material : EmissiveMaterials)
	{
		Material->SetScalarParameterValue(TEXT("EmissiveIntensity"), bInActive ? 2.f : 0.f);
	}
	bLightInRange = bInLightVisible;
	TargetVisualAlpha = bInActive ? 1.f : 0.f;
	if (!bVisualInitialized)
	{
		VisualAlpha = TargetVisualAlpha;
		bVisualInitialized = true;
		Tick(0.f);
	}
	SetActorTickEnabled(GetNetMode() != NM_DedicatedServer && !FMath::IsNearlyEqual(VisualAlpha, TargetVisualAlpha));
}

void AVoxelWorldObjectPresenter::Tick(float InDeltaSeconds)
{
	Super::Tick(InDeltaSeconds);
	if (!Definition)
	{
		SetActorTickEnabled(false);
		return;
	}
	VisualAlpha = FMath::FInterpConstantTo(VisualAlpha, TargetVisualAlpha, InDeltaSeconds, 1.f / FMath::Max(0.01f, Definition->TransitionSeconds));
	for (int32 Index = 0; Index < VisualMeshes.Num(); ++Index)
	{
		const FVoxelWorldObjectVisualPart& Part = Definition->VisualParts[Index];
		VisualMeshes[Index]->SetRelativeLocation(FMath::Lerp(Part.ClosedOffset, Part.OpenOffset, VisualAlpha));
		VisualMeshes[Index]->SetRelativeRotation(FQuat::Slerp(FQuat::Identity, Part.OpenRotation.Quaternion(), VisualAlpha));
	}
	if (FMath::IsNearlyEqual(VisualAlpha, TargetVisualAlpha))
	{
		SetActorTickEnabled(false);
	}
}
