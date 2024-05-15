// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Asset/Modifier/AssetModifierBase.h"
#include "AssetModifier_RemoveMaterialUserData.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class WHFRAMEWORKEDITOR_API UAssetModifier_RemoveMaterialUserData : public UAssetModifierBase
{
	GENERATED_BODY()

public:
	UAssetModifier_RemoveMaterialUserData();

public:
	virtual bool CanModify_Implementation(const FAssetData& InAssetData) const override;

	virtual void DoModify_Implementation(const FAssetData& InAssetData) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UAssetUserData> UserDataClass;
};
