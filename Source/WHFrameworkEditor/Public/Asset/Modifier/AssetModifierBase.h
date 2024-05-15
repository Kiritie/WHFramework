// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Common/Base/WHObject.h"

#include "AssetModifierBase.generated.h"

//////////////////////////////////////////////////////////////////////////
// EditorSettings
UCLASS(Abstract, Blueprintable, BlueprintType)
class WHFRAMEWORKEDITOR_API UAssetModifierBase : public UWHObject
{
	GENERATED_BODY()

public:
	UAssetModifierBase();

public:
	UFUNCTION(BlueprintNativeEvent)
	bool CanModify(const FAssetData& InAssetData) const;

	UFUNCTION(BlueprintNativeEvent)
	void DoModify(const FAssetData& InAssetData);
};
