// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/ProcedureEditorSettings.h"

UProcedureEditorSettings::UProcedureEditorSettings()
{
	bShowListPanel = true;
	bShowDetailPanel = true;
	bShowStatusPanel = true;

	bDefaultIsMultiMode = true;
	bDefaultIsEditMode = false;
}
