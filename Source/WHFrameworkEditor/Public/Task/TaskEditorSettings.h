// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TaskEditorSettings.generated.h"

/**
 * 
 */
UCLASS(config = TaskEditor, configdonotcheckdefaults)
class WHFRAMEWORKEDITOR_API UTaskEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UTaskEditorSettings();

public:
	UPROPERTY(Config, EditAnywhere, Category = "List")
	bool bDefaultIsMultiMode;

	UPROPERTY(Config, EditAnywhere, Category = "List")
    bool bDefaultIsEditMode;
};
