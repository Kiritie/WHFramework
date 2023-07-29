// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProcedureEditorSettings.generated.h"

/**
 * 
 */
UCLASS(config = ProcedureEditor, configdonotcheckdefaults)
class WHFRAMEWORKEDITOR_API UProcedureEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UProcedureEditorSettings();

public:
	UPROPERTY(Config, EditAnywhere, Category = "List")
	bool bDefaultIsMultiMode;

	UPROPERTY(Config, EditAnywhere, Category = "List")
    bool bDefaultIsEditMode;
};
