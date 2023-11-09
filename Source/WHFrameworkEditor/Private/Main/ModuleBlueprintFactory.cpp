// Copyright Epic Games, Inc. All Rights Reserved.

#include "Main/ModuleBlueprintFactory.h"

#include "BlueprintEditorSettings.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Main/ModuleGraph.h"
#include "Main/ModuleGraphSchema.h"
#include "Main/Base/ModuleBase.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Main/Base/ModuleBlueprint.h"

#define LOCTEXT_NAMESPACE "ModuleBlueprintFactory"

UModuleBlueprintFactory::UModuleBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UModuleBlueprint::StaticClass();
	ParentClass = UModuleBase::StaticClass();
}

bool UModuleBlueprintFactory::ConfigureProperties()
{
	return Super::ConfigureProperties();
};

UObject* UModuleBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a Module blueprint, then create and init one
	check(Class->IsChildOf<UModuleBlueprint>());

	// If they selected an interface, force the parent class to be UInterface
	if(BlueprintType == BPTYPE_Interface) { ParentClass = UInterface::StaticClass(); }

	if((ParentClass == NULL) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass) || !ParentClass->IsChildOf<UModuleBase>())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString(ParentClass->GetName()) : LOCTEXT("Null", "(null)"));
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("CannotCreateModuleBlueprint", "Cannot create a Module Blueprint based on the class '{ClassName}'."), Args));
		return NULL;
	}
	else
	{
		UModuleBlueprint* NewBP = CastChecked<UModuleBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BlueprintType, UModuleBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext));

		if(NewBP)
		{
			UModuleBlueprint* ModuleBP = UModuleBlueprint::FindModuleBlueprint(NewBP);
			if(ModuleBP == NULL)
			{
				const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

				// Only allow a Module graph if there isn't one in a parent blueprint
				UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(NewBP, TEXT("Module Graph"), UModuleGraph::StaticClass(), UModuleGraphSchema::StaticClass());
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
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnInitialize")), UModuleBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnEnter")), UModuleBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnRefresh")), UModuleBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnGuide")), UModuleBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnLeave")), UModuleBase::StaticClass(), NodePositionY);
				}
			}
		}

		return NewBP;
	}
}

UObject* UModuleBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return Super::FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn);
}

#undef LOCTEXT_NAMESPACE
