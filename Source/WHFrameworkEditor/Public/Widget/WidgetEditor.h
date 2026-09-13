// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Main/Module/EditorModuleBase.h"
#include "WHFrameworkEditorTypes.h"

class FWidgetEditorModule : public FEditorModuleBase
{
	GENERATED_EDITOR_MODULE(FWidgetEditorModule)

public:
	FWidgetEditorModule();

public:
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

	virtual void RegisterCustomization(FPropertyEditorModule& PropertyEditor) override;

	virtual void UnRegisterCustomization(FPropertyEditorModule& PropertyEditor) override;
};
