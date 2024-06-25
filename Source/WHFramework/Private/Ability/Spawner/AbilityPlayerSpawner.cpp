// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Spawner/AbilityPlayerSpawner.h"

#include "Ability/AbilityModuleTypes.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"

AAbilityPlayerSpawner::AAbilityPlayerSpawner()
{
	GetCapsuleComponent()->InitCapsuleSize(40.0f, 92.0f);

#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> PlayerStartTextureObject;
			FName ID_PlayerStart;
			FText NAME_PlayerStart;
			FName ID_Navigation;
			FText NAME_Navigation;
			FConstructorStatics()
				: PlayerStartTextureObject(TEXT("/Engine/EditorResources/S_Player"))
				, ID_PlayerStart(TEXT("PlayerStart"))
				, NAME_PlayerStart(NSLOCTEXT("SpriteCategory", "PlayerStart", "Player Start"))
				, ID_Navigation(TEXT("Navigation"))
				, NAME_Navigation(NSLOCTEXT("SpriteCategory", "Navigation", "Navigation"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (GetGoodSprite())
		{
			GetGoodSprite()->Sprite = ConstructorStatics.PlayerStartTextureObject.Get();
			GetGoodSprite()->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
			GetGoodSprite()->SpriteInfo.Category = ConstructorStatics.ID_PlayerStart;
			GetGoodSprite()->SpriteInfo.DisplayName = ConstructorStatics.NAME_PlayerStart;
		}
		if (GetBadSprite())
		{
			GetBadSprite()->SetVisibility(false);
		}
		if (GetArrowComponent())
		{
			GetArrowComponent()->SpriteInfo.Category = ConstructorStatics.ID_Navigation;
			GetArrowComponent()->SpriteInfo.DisplayName = ConstructorStatics.NAME_Navigation;
		}
	}
#endif
}

AActor* AAbilityPlayerSpawner::SpawnImpl_Implementation(const FAbilityItem& InAbilityItem)
{
	return nullptr;
}

void AAbilityPlayerSpawner::DestroyImpl_Implementation(AActor* InAbilityActor)
{
	Super::DestroyImpl_Implementation(InAbilityActor);
}
