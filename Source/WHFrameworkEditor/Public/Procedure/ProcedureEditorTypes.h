#pragma once

#include "CoreMinimal.h"
#include "WHFrameworkEditorTypes.h"
#include "WHFrameworkEditorStyle.h"
#include "Framework/Commands/Commands.h"

class FProcedureClassFilter : public FClassViewerFilterBase
{
public:
    FProcedureClassFilter();
    virtual bool IsClassAllowed(UClass* InClass) override;
};

class FProcedureEditorCommands : public TCommands<FProcedureEditorCommands>
{
public:
    FProcedureEditorCommands();
    virtual void RegisterCommands() override;
    TSharedPtr<FUICommandInfo> OpenProcedureEditorWindow;
};
