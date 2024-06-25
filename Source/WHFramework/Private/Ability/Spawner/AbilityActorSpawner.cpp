﻿// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Spawner/AbilityActorSpawner.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/AbilityModuleTypes.h"
#include "Ability/Actor/AbilityActorBase.h"
#include "Ability/Actor/AbilityActorDataBase.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"

AAbilityActorSpawner::AAbilityActorSpawner()
{
	GetCapsuleComponent()->InitCapsuleSize(40.0f, 40.0f);

#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> ActorTextureObject;
			FName ID_Actor;
			FText NAME_Actor;
			FName ID_Navigation;
			FText NAME_Navigation;
			FConstructorStatics()
				: ActorTextureObject(TEXT("/Engine/EditorResources/S_Actor"))
				, ID_Actor(TEXT("Actor"))
				, NAME_Actor(NSLOCTEXT("SpriteCategory", "Actor", "Actor"))
				, ID_Navigation(TEXT("Navigation"))
				, NAME_Navigation(NSLOCTEXT("SpriteCategory", "Navigation", "Navigation"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (GetGoodSprite())
		{
			GetGoodSprite()->Sprite = ConstructorStatics.ActorTextureObject.Get();
			GetGoodSprite()->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
			GetGoodSprite()->SpriteInfo.Category = ConstructorStatics.ID_Actor;
			GetGoodSprite()->SpriteInfo.DisplayName = ConstructorStatics.NAME_Actor;
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

AActor* AAbilityActorSpawner::SpawnImpl_Implementation(const FAbilityItem& InAbilityItem)
{
	const auto& ActorData = InAbilityItem.GetData<UAbilityActorDataBase>();
	
	auto SaveData = FActorSaveData();
	SaveData.AssetID = ActorData.GetPrimaryAssetId();
	SaveData.Name = *ActorData.Name.ToString();
	SaveData.Level = InAbilityItem.Level;
	SaveData.SpawnLocation = GetActorLocation();
	SaveData.SpawnRotation = GetActorRotation();
	SaveData.InventoryData = ActorData.InventoryData;

	AAbilityActorBase* Actor = UAbilityModuleStatics::SpawnAbilityActor(&SaveData);

	return Actor;
}

void AAbilityActorSpawner::DestroyImpl_Implementation(AActor* InAbilityActor)
{
	Super::DestroyImpl_Implementation(InAbilityActor);
}
