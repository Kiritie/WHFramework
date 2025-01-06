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
class WHFRAMEWORKEDITOR_API UEditorStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	// Asset
	UFUNCTION(BlueprintCallable, Category = "EditorStatics")
	static UObject* CreateAssetWithDialog(TSubclassOf<UFactory> InFactoryClass);

	UFUNCTION(BlueprintCallable, Category = "EditorStatics")
	static UBlueprint* CreateBlueprintAssetWithDialog(TSubclassOf<UBlueprintFactoryBase> InFactoryClass, bool bAutoOpenAsset = true);
};
