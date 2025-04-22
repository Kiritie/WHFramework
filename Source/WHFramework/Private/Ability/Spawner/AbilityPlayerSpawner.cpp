// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Spawner/AbilityPlayerSpawner.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/AbilityModuleTypes.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/AbilityCharacterDataBase.h"
#include "Ability/Pawn/AbilityPawnBase.h"
#include "Character/CharacterModuleStatics.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Pawn/PawnModuleStatics.h"

AAbilityPlayerSpawner::AAbilityPlayerSpawner()
{
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
	const auto& CharacterData = InAbilityItem.GetData<UAbilityCharacterDataBase>();
	
	auto SaveData = FCharacterSaveData();
	SaveData.AssetID = CharacterData.GetPrimaryAssetId();
	SaveData.Name = *CharacterData.Name.ToString();
	SaveData.RaceID = CharacterData.RaceID;
	SaveData.Level = InAbilityItem.Level;
	SaveData.SpawnTransform = GetActorTransform();
	SaveData.InventoryData = CharacterData.InventoryData;
	
	AActor* PlayerActor = UAbilityModuleStatics::SpawnAbilityActor(&SaveData);
	if(AAbilityPawnBase* PlayerPawn = Cast<AAbilityPawnBase>(PlayerActor))
	{
		UPawnModuleStatics::SwitchPawn(PlayerPawn);
	}
	else if(AAbilityCharacterBase* PlayerCharacter = Cast<AAbilityCharacterBase>(PlayerActor))
	{
		UCharacterModuleStatics::SwitchCharacter(PlayerCharacter);
	}
	return PlayerActor;
}

void AAbilityPlayerSpawner::DestroyImpl_Implementation(AActor* InAbilityActor)
{
	Super::DestroyImpl_Implementation(InAbilityActor);
}
