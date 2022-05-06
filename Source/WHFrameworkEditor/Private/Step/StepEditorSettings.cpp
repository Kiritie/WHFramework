// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/StepEditorSettings.h"

UStepEditorSettings::UStepEditorSettings()
{
	bShowListPanel = true;
	bShowDetailPanel = true;
	bShowStatusPanel = true;

	bDefaultIsMultiMode = true;
	bDefaultIsEditMode = false;
}
