// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/Blueprint/BlueprintFactoryBase.h"

#include "BlueprintEditorSettings.h"
#include "Common/CommonEditorTypes.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "BlueprintFactoryBase"

UBlueprintFactoryBase::UBlueprintFactoryBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BlueprintType = BPTYPE_Normal;

	GraphClass = nullptr;
	GraphSchemaClass = nullptr;
	DefaultEventGraph = FName("Event Graph");
	DefaultEventNodes = TArray<FName>();

	ClassPickerTitle = LOCTEXT("CreateAssetOptions", "Pick Class For Blueprint Instance");
}

UObject* UBlueprintFactoryBase::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a blueprint, then create and init one
	check(Class->IsChildOf(SupportedClass));

	// If they selected an interface, force the parent class to be UInterface
	if(BlueprintType == BPTYPE_Interface) { PickedClass = UInterface::StaticClass(); }

	if((PickedClass == NULL) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(PickedClass))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ClassName"), (PickedClass != NULL) ? FText::FromString(PickedClass->GetName()) : LOCTEXT("Null", "(null)"));
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("CannotCreateBlueprint", "Cannot create a Blueprint based on the class '{ClassName}'."), Args));
		return NULL;
	}
	else
	{
		UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(PickedClass, InParent, Name, BlueprintType, SupportedClass.Get(), UBlueprintGeneratedClass::StaticClass(), CallingContext);

		if(NewBP)
		{
			// Only allow a event graph if there isn't one in a parent blueprint
			UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(NewBP, DefaultEventGraph, GraphClass, GraphSchemaClass);
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
				for(const auto Iter : DefaultEventNodes)
				{
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, Iter, PickedClass, NodePositionY);
				}
			}
		}

		return NewBP;
	}
}

UObject* UBlueprintFactoryBase::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return Super::FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn);
}

#undef LOCTEXT_NAMESPACE
