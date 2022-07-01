// Copyright Epic Games, Inc. All Rights Reserved.

#include "FSM/FiniteStateBlueprintFactory.h"

#include "BlueprintEditorSettings.h"
#include "ClassViewerFilter.h"
#include "K2Node_Event.h"
#include "SlateOptMacros.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "FSM/FiniteStateGraph.h"
#include "FSM/FiniteStateGraphSchema.h"
#include "FSM/Base/FiniteStateBase.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "FSM/Base/FiniteStateBlueprint.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Debug/DebugModuleTypes.h"

#define LOCTEXT_NAMESPACE "FiniteStateBlueprintFactory"

UFiniteStateBlueprintFactory::UFiniteStateBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UFiniteStateBlueprint::StaticClass();
	ParentClass = UFiniteStateBase::StaticClass();
}

bool UFiniteStateBlueprintFactory::ConfigureProperties()
{
	return Super::ConfigureProperties();
};

UObject* UFiniteStateBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a FiniteState blueprint, then create and init one
	check(Class->IsChildOf(UFiniteStateBlueprint::StaticClass()));

	// If they selected an interface, force the parent class to be UInterface
	if(BlueprintType == BPTYPE_Interface) { ParentClass = UInterface::StaticClass(); }

	if((ParentClass == NULL) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass) || !ParentClass->IsChildOf(UFiniteStateBase::StaticClass()))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString(ParentClass->GetName()) : LOCTEXT("Null", "(null)"));
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("CannotCreateFiniteStateBlueprint", "Cannot create a FiniteState Blueprint based on the class '{ClassName}'."), Args));
		return NULL;
	}
	else
	{
		UFiniteStateBlueprint* NewBP = CastChecked<UFiniteStateBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BlueprintType, UFiniteStateBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext));

		if(NewBP)
		{
			UFiniteStateBlueprint* FiniteStateBP = UFiniteStateBlueprint::FindFiniteStateBlueprint(NewBP);
			if(FiniteStateBP == NULL)
			{
				const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

				// Only allow a FiniteState graph if there isn't one in a parent blueprint
				UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(NewBP, TEXT("FiniteState Graph"), UFiniteStateGraph::StaticClass(), UFiniteStateGraphSchema::StaticClass());
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
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnInitialize")), UFiniteStateBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnEnter")), UFiniteStateBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnRefresh")), UFiniteStateBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnGuide")), UFiniteStateBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnLeave")), UFiniteStateBase::StaticClass(), NodePositionY);
				}
			}
		}

		return NewBP;
	}
}

UObject* UFiniteStateBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return Super::FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn);
}

#undef LOCTEXT_NAMESPACE
