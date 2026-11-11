#include "Ability/Inventory/InventoryItemPresentation.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

UMeshComponent* FInventoryMeshData::CreateComponent(AActor* InOwner) const
{
    if (!InOwner || !Mesh) return nullptr;
    UMeshComponent* Component = nullptr;
    if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Mesh))
    {
        UStaticMeshComponent* StaticComponent = NewObject<UStaticMeshComponent>(InOwner);
        StaticComponent->SetStaticMesh(StaticMesh);
        Component = StaticComponent;
    }
    else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Mesh))
    {
        USkeletalMeshComponent* SkeletalComponent = NewObject<USkeletalMeshComponent>(InOwner);
        SkeletalComponent->SetSkeletalMesh(SkeletalMesh);
        if (AnimationClass) SkeletalComponent->SetAnimInstanceClass(AnimationClass);
        Component = SkeletalComponent;
    }
    if (Component)
    {
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCanEverAffectNavigation(false);
        if (MaterialOverride) Component->SetMaterial(0, MaterialOverride);
    }
    return Component;
}
