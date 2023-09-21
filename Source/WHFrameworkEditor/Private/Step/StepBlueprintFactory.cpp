// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/StepBlueprintFactory.h"

#include "BlueprintEditorSettings.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Step/StepGraph.h"
#include "Step/StepGraphSchema.h"
#include "Step/Base/StepBase.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Step/Base/StepBlueprint.h"

#define LOCTEXT_NAMESPACE "StepBlueprintFactory"

UStepBlueprintFactory::UStepBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UStepBlueprint::StaticClass();
	ParentClass = UStepBase::StaticClass();
}

bool UStepBlueprintFactory::ConfigureProperties()
{
	return Super::ConfigureProperties();
};

UObject* UStepBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a Step blueprint, then create and init one
	check(Class->IsChildOf<UStepBlueprint>());

	// If they selected an interface, force the parent class to be UInterface
	if(BlueprintType == BPTYPE_Interface) { ParentClass = UInterface::StaticClass(); }

	if((ParentClass == NULL) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass) || !ParentClass->IsChildOf<UStepBase>())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString(ParentClass->GetName()) : LOCTEXT("Null", "(null)"));
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("CannotCreateStepBlueprint", "Cannot create a Step Blueprint based on the class '{ClassName}'."), Args));
		return NULL;
	}
	else
	{
		UStepBlueprint* NewBP = CastChecked<UStepBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BlueprintType, UStepBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext));

		if(NewBP)
		{
			UStepBlueprint* StepBP = UStepBlueprint::FindRootStepBlueprint(NewBP);
			if(StepBP == NULL)
			{
				const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

				// Only allow a Step graph if there isn't one in a parent blueprint
				UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(NewBP, TEXT("Step Graph"), UStepGraph::StaticClass(), UStepGraphSchema::StaticClass());
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
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnInitialize")), UStepBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnRestore")), UStepBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnEnter")), UStepBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnRefresh")), UStepBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnGuide")), UStepBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnExecute")), UStepBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnComplete")), UStepBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnLeave")), UStepBase::StaticClass(), NodePositionY);
				}
			}
		}

		return NewBP;
	}
}

UObject* UStepBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return Super::FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn);
}

#undef LOCTEXT_NAMESPACE
