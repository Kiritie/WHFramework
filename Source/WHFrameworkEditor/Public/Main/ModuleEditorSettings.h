// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ModuleEditorSettings.generated.h"

/**
 * 
 */
UCLASS(config = ModuleEditor, configdonotcheckdefaults)
class WHFRAMEWORKEDITOR_API UModuleEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UModuleEditorSettings();

public:
	UPROPERTY(Config, EditAnywhere, Category = "List")
	bool bDefaultIsMultiMode;

	UPROPERTY(Config, EditAnywhere, Category = "List")
    bool bDefaultIsEditMode;
};
