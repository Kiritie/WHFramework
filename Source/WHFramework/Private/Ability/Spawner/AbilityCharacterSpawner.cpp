// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Spawner/AbilityCharacterSpawner.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/AbilityModuleTypes.h"
#include "Ability/Actor/AbilityActorBase.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/AbilityCharacterDataBase.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"

AAbilityCharacterSpawner::AAbilityCharacterSpawner()
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> CharacterTextureObject;
			FName ID_Character;
			FText NAME_Character;
			FName ID_Navigation;
			FText NAME_Navigation;
			FConstructorStatics()
				: CharacterTextureObject(TEXT("/Engine/EditorResources/S_Pawn"))
				, ID_Character(TEXT("Character"))
				, NAME_Character(NSLOCTEXT("SpriteCategory", "Character", "Character"))
				, ID_Navigation(TEXT("Navigation"))
				, NAME_Navigation(NSLOCTEXT("SpriteCategory", "Navigation", "Navigation"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (GetGoodSprite())
		{
			GetGoodSprite()->Sprite = ConstructorStatics.CharacterTextureObject.Get();
			GetGoodSprite()->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
			GetGoodSprite()->SpriteInfo.Category = ConstructorStatics.ID_Character;
			GetGoodSprite()->SpriteInfo.DisplayName = ConstructorStatics.NAME_Character;
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

AActor* AAbilityCharacterSpawner::SpawnImpl_Implementation(const FAbilityItem& InAbilityItem)
{
	const auto& CharacterData = InAbilityItem.GetData<UAbilityCharacterDataBase>();
	
	auto SaveData = FCharacterSaveData();
	SaveData.AssetID = CharacterData.GetPrimaryAssetId();
	SaveData.Name = *CharacterData.Name.ToString();
	SaveData.RaceID = CharacterData.RaceID;
	SaveData.Level = InAbilityItem.Level;
	SaveData.SpawnLocation = GetActorLocation();
	SaveData.SpawnRotation = GetActorRotation();
	SaveData.InventoryData = CharacterData.InventoryData;

	return UAbilityModuleStatics::SpawnAbilityActor(&SaveData);
}

void AAbilityCharacterSpawner::DestroyImpl_Implementation(AActor* InAbilityActor)
{
	Super::DestroyImpl_Implementation(InAbilityActor);
}
