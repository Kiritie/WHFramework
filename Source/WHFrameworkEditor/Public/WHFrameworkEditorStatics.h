// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "WHFrameworkEditorStatics.generated.h"

class UFactory;
class UBlueprintFactoryBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORKEDITOR_API UWHFrameworkEditorStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	// Asset
	UFUNCTION(BlueprintCallable, Category = "WHFrameworkEditorStatics")
	static UObject* CreateAssetWithDialog(TSubclassOf<UFactory> InFactoryClass);

	UFUNCTION(BlueprintCallable, Category = "WHFrameworkEditorStatics")
	static UBlueprint* CreateBlueprintAssetWithDialog(TSubclassOf<UBlueprintFactoryBase> InFactoryClass, bool bAutoOpenAsset = true);
};
