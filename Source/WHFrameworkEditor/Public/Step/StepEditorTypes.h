#pragma once

#include "CoreMinimal.h"
#include "Step/StepEditor.h"
#include "WHFrameworkEditorStyle.h"

class FStepClassFilter : public FClassViewerFilterBase
{
public:
	FStepClassFilter();
	TWeakPtr<FStepEditor> StepEditor;
	virtual bool IsClassAllowed(UClass* InClass) override;
};

class FStepEditorCommands : public TCommands<FStepEditorCommands>
{
public:
	FStepEditorCommands();
	virtual void RegisterCommands() override;
	TSharedPtr<FUICommandInfo> OpenStepEditorWindow;
};
