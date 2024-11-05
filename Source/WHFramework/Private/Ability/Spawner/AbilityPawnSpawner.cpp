// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Spawner/AbilityPawnSpawner.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/AbilityModuleTypes.h"
#include "Ability/Pawn/AbilityPawnBase.h"
#include "Ability/Pawn/AbilityPawnDataBase.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"

AAbilityPawnSpawner::AAbilityPawnSpawner()
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> PawnTextureObject;
			FName ID_Pawn;
			FText NAME_Pawn;
			FName ID_Navigation;
			FText NAME_Navigation;
			FConstructorStatics()
				: PawnTextureObject(TEXT("/Engine/EditorResources/S_Pawn"))
				, ID_Pawn(TEXT("Pawn"))
				, NAME_Pawn(NSLOCTEXT("SpriteCategory", "Pawn", "Pawn"))
				, ID_Navigation(TEXT("Navigation"))
				, NAME_Navigation(NSLOCTEXT("SpriteCategory", "Navigation", "Navigation"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (GetGoodSprite())
		{
			GetGoodSprite()->Sprite = ConstructorStatics.PawnTextureObject.Get();
			GetGoodSprite()->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
			GetGoodSprite()->SpriteInfo.Category = ConstructorStatics.ID_Pawn;
			GetGoodSprite()->SpriteInfo.DisplayName = ConstructorStatics.NAME_Pawn;
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

AActor* AAbilityPawnSpawner::SpawnImpl_Implementation(const FAbilityItem& InAbilityItem)
{
	const auto& PawnData = InAbilityItem.GetData<UAbilityPawnDataBase>();
	
	auto SaveData = FPawnSaveData();
	SaveData.AssetID = PawnData.GetPrimaryAssetId();
	SaveData.Name = *PawnData.Name.ToString();
	SaveData.RaceID = PawnData.RaceID;
	SaveData.Level = InAbilityItem.Level;
	SaveData.SpawnTransform = GetActorTransform();
	SaveData.InitInventoryData();

	return UAbilityModuleStatics::SpawnAbilityActor(&SaveData);
}

void AAbilityPawnSpawner::DestroyImpl_Implementation(AActor* InAbilityActor)
{
	Super::DestroyImpl_Implementation(InAbilityActor);
}
