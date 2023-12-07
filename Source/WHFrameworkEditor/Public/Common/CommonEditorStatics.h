// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "CommonEditorStatics.generated.h"

class UFactory;
class UBlueprintFactoryBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORKEDITOR_API UCommonEditorStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CommonEditorStatics")
	static UObject* CreateAssetWithDialog(TSubclassOf<UFactory> InFactoryClass);

	UFUNCTION(BlueprintCallable, Category = "CommonEditorStatics")
	static UBlueprint* CreateBlueprintAssetWithDialog(TSubclassOf<UBlueprintFactoryBase> InFactoryClass, bool bAutoOpenAsset = true);
};
