// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WHProcedureEditorSettings.generated.h"

/**
 * 
 */
UCLASS(config = ProcedureEditor, configdonotcheckdefaults)
class WHPROCEDUREEDITOR_API UWHProcedureEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UWHProcedureEditorSettings();

public:
	UPROPERTY(Config, EditAnywhere, Category = "View")
	bool bShowListPanel;

	UPROPERTY(Config, EditAnywhere, Category = "View")
	bool bShowDetailPanel;

	UPROPERTY(Config, EditAnywhere, Category = "View")
	bool bShowStatusPanel;

	UPROPERTY(Config, EditAnywhere, Category = "List")
	bool bDefaultIsMultiMode;

	UPROPERTY(Config, EditAnywhere, Category = "List")
    bool bDefaultIsEditMode;
};
