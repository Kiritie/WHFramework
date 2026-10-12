#include "Procedure/ProcedureEditorTypes.h"

#include "Procedure/Base/ProcedureBase.h"

FProcedureClassFilter::FProcedureClassFilter()
{
    DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown;
    AllowedChildrenOfClasses.Add(UProcedureBase::StaticClass());
    bIncludeBaseClasses = true;
    bIncludeParentClasses = true;
}

bool FProcedureClassFilter::IsClassAllowed(UClass* InClass)
{
    return InClass && InClass->IsChildOf(UProcedureBase::StaticClass()) &&
        !InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown);
}

FProcedureEditorCommands::FProcedureEditorCommands()
    : TCommands<FProcedureEditorCommands>(TEXT("ProcedureEditor"), NSLOCTEXT("ProcedureGraph", "Commands", "流程图编辑器"), NAME_None, FWHFrameworkEditorStyle::GetStyleSetName())
{
}

void FProcedureEditorCommands::RegisterCommands()
{
#define LOCTEXT_NAMESPACE "ProcedureEditorCommands"
    UI_COMMAND(OpenProcedureEditorWindow, "Procedure Editor", "打开流程图编辑器", EUserInterfaceActionType::Button, FInputChord());
#undef LOCTEXT_NAMESPACE
}
