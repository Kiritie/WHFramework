// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StepEditorSettings.generated.h"

/**
 * 
 */
UCLASS(config = StepEditor, configdonotcheckdefaults)
class WHFRAMEWORKEDITOR_API UStepEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UStepEditorSettings();

public:
	UPROPERTY(Config, EditAnywhere, Category = "List")
	bool bDefaultIsMultiMode;

	UPROPERTY(Config, EditAnywhere, Category = "List")
    bool bDefaultIsEditMode;
};
