// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/TaskEditorSettings.h"

UTaskEditorSettings::UTaskEditorSettings()
{
	bShowListPanel = true;
	bShowDetailPanel = true;
	bShowStatusPanel = true;

	bDefaultIsMultiMode = true;
	bDefaultIsEditMode = false;
}
