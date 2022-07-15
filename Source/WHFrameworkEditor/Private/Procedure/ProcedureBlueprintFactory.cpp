// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/ProcedureBlueprintFactory.h"

#include "BlueprintEditorSettings.h"
#include "ClassViewerFilter.h"
#include "K2Node_Event.h"
#include "SlateOptMacros.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Procedure/ProcedureGraph.h"
#include "Procedure/ProcedureGraphSchema.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Procedure/Base/ProcedureBlueprint.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Debug/DebugModuleTypes.h"

#define LOCTEXT_NAMESPACE "ProcedureBlueprintFactory"

UProcedureBlueprintFactory::UProcedureBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UProcedureBlueprint::StaticClass();
	ParentClass = UProcedureBase::StaticClass();
}

bool UProcedureBlueprintFactory::ConfigureProperties()
{
	return Super::ConfigureProperties();
};

UObject* UProcedureBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a Procedure blueprint, then create and init one
	check(Class->IsChildOf<UProcedureBlueprint>());

	// If they selected an interface, force the parent class to be UInterface
	if(BlueprintType == BPTYPE_Interface) { ParentClass = UInterface::StaticClass(); }

	if((ParentClass == NULL) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass) || !ParentClass->IsChildOf<UProcedureBase>())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString(ParentClass->GetName()) : LOCTEXT("Null", "(null)"));
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("CannotCreateProcedureBlueprint", "Cannot create a Procedure Blueprint based on the class '{ClassName}'."), Args));
		return NULL;
	}
	else
	{
		UProcedureBlueprint* NewBP = CastChecked<UProcedureBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BlueprintType, UProcedureBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext));

		if(NewBP)
		{
			UProcedureBlueprint* ProcedureBP = UProcedureBlueprint::FindProcedureBlueprint(NewBP);
			if(ProcedureBP == NULL)
			{
				const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

				// Only allow a Procedure graph if there isn't one in a parent blueprint
				UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(NewBP, TEXT("Procedure Graph"), UProcedureGraph::StaticClass(), UProcedureGraphSchema::StaticClass());
				#if WITH_EDITORONLY_DATA
				if(NewBP->UbergraphPages.Num()) { FBlueprintEditorUtils::RemoveGraphs(NewBP, NewBP->UbergraphPages); }
				#endif
				FBlueprintEditorUtils::AddUbergraphPage(NewBP, NewGraph);
				NewBP->LastEditedDocuments.Add(NewGraph);
				NewGraph->bAllowDeletion = false;

				UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
				if(Settings && Settings->bSpawnDefaultBlueprintNodes)
				{
					int32 NodePositionY = 0;
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnInitialize")), UProcedureBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnEnter")), UProcedureBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnRefresh")), UProcedureBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnGuide")), UProcedureBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnLeave")), UProcedureBase::StaticClass(), NodePositionY);
				}
			}
		}

		return NewBP;
	}
}

UObject* UProcedureBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return Super::FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn);
}

#undef LOCTEXT_NAMESPACE
