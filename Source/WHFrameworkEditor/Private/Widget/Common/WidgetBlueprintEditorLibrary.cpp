#include "Widget/Common/WidgetBlueprintEditorLibrary.h"

#include "Blueprint/WidgetTree.h"
#include "Animation/WidgetAnimation.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "EdGraphUtilities.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "K2Node_ComponentBoundEvent.h"
#include "UObject/UnrealType.h"
#include "ScopedTransaction.h"
#include "Misc/ScopeExit.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/NamedSlotInterface.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditorUtils.h"
#include "WidgetBlueprintOperationUtils.h"

UWidgetTree* UWidgetBlueprintEditorLibrary::GetWidgetTree(UBlueprint* InBlueprint)
{
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(InBlueprint);
	return Blueprint ? Blueprint->WidgetTree.Get() : nullptr;
}

UWidget* UWidgetBlueprintEditorLibrary::GetRootWidget(UBlueprint* InBlueprint)
{
	const UWidgetTree* Tree = GetWidgetTree(InBlueprint);
	return Tree ? Tree->RootWidget.Get() : nullptr;
}

UWidget* UWidgetBlueprintEditorLibrary::FindWidget(UBlueprint* InBlueprint, FName InName)
{
	const UWidgetTree* Tree = GetWidgetTree(InBlueprint);
	return Tree ? Tree->FindWidget(InName) : nullptr;
}

TArray<UWidget*> UWidgetBlueprintEditorLibrary::GetWidgets(UBlueprint* InBlueprint)
{
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(InBlueprint);
	return Blueprint ? Blueprint->GetAllSourceWidgets() : TArray<UWidget*>();
}

FString UWidgetBlueprintEditorLibrary::ExportGraphData(UBlueprint* InBlueprint)
{
	if (!InBlueprint)
	{
		return FString();
	}
	TArray<UEdGraph*> Graphs;
	InBlueprint->GetAllGraphs(Graphs);
	TArray<TSharedPtr<FJsonValue>> GraphData;
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}
		TSet<UObject*> Nodes;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
			{
				Nodes.Add(Node);
			}
		}
		FString NodeText;
		FEdGraphUtilities::ExportNodesToText(Nodes, NodeText);
		TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
		Record->SetStringField(TEXT("graph"), Graph->GetPathName());
		Record->SetNumberField(TEXT("node_count"), Nodes.Num());
		Record->SetStringField(TEXT("nodes"), NodeText);
		GraphData.Add(MakeShared<FJsonValueObject>(Record));
	}
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprint"), InBlueprint->GetPathName());
	Data->SetArrayField(TEXT("graphs"), GraphData);
	FString Result;
	FJsonSerializer::Serialize(Data, TJsonWriterFactory<>::Create(&Result));
	return Result;
}

UWidget* UWidgetBlueprintEditorLibrary::CreateWidgetTemplate(UBlueprint* InBlueprint, TSubclassOf<UWidget> InClass, FName InName, FString& OutError)
{
	OutError.Reset();
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(InBlueprint);
	UWidgetTree* Tree = GetWidgetTree(InBlueprint);
	if (!Tree || !InClass || InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) ||
		InName.IsNone() || FindObject<UObject>(Tree, *InName.ToString()) || Blueprint->WidgetVariableNameToGuidMap.Contains(InName))
	{
		OutError = TEXT("A source tree, concrete widget class and unused widget name are required.");
		return nullptr;
	}
	FScopedTransaction Transaction(NSLOCTEXT("WidgetBlueprint", "CreateTemplate", "Create widget template"));
	InBlueprint->Modify();
	Tree->Modify();
	UWidget* Widget = NewObject<UWidget>(Tree, InClass, InName, RF_Transactional);
	Blueprint->OnVariableAdded(InName);
	InBlueprint->MarkPackageDirty();
	return Widget;
}

bool UWidgetBlueprintEditorLibrary::SetRootWidget(UBlueprint* InBlueprint, UWidget* InRoot, FString& OutError)
{
	OutError.Reset();
	UWidgetTree* Tree = GetWidgetTree(InBlueprint);
	if (!Tree || (InRoot && (InRoot->GetOuter() != Tree || InRoot->GetParent())))
	{
		OutError = TEXT("Root must be an unparented widget template owned by this source tree.");
		return false;
	}
	FScopedTransaction Transaction(NSLOCTEXT("WidgetBlueprint", "SetRoot", "Set widget root"));
	InBlueprint->Modify();
	Tree->Modify();
	Tree->RootWidget = InRoot;
	InBlueprint->MarkPackageDirty();
	return true;
}

bool UWidgetBlueprintEditorLibrary::MoveWidget(UBlueprint* InBlueprint, UWidget* InWidget, UPanelWidget* InParent, int32 InChildIndex, FString& OutError)
{
	OutError.Reset();
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(InBlueprint);
	UWidgetTree* Tree = GetWidgetTree(InBlueprint);
	if (!Tree || !InWidget || !InParent || InWidget->GetOuter() != Tree || InParent->GetOuter() != Tree ||
		Tree->RootWidget == InWidget || InChildIndex < INDEX_NONE || InChildIndex > InParent->GetChildrenCount())
	{
		OutError = TEXT("移动对象和目标容器必须属于同一源控件树，根控件和越界索引不能移动。");
		return false;
	}
	FScopedTransaction Transaction(NSLOCTEXT("WidgetBlueprint", "MoveSourceTemplate", "Move widget template"));
	Blueprint->Modify();
	Tree->Modify();
	const bool bPreviouslySuppressed = Blueprint->bSuppressStructurallyModified;
	Blueprint->bSuppressStructurallyModified = true;
	ON_SCOPE_EXIT
	{
		Blueprint->bSuppressStructurallyModified = bPreviouslySuppressed;
	};
	FText Error;
	if (!FWidgetBlueprintOperationUtils::MoveWidget(Blueprint, InWidget, InParent, InChildIndex, Error))
	{
		Transaction.Cancel();
		OutError = Error.ToString();
		return false;
	}
	Blueprint->MarkPackageDirty();
	return true;
}

bool UWidgetBlueprintEditorLibrary::ReplaceWidget(UBlueprint* InBlueprint, UWidget* InWidget, TSubclassOf<UWidget> InClass, FString& OutError)
{
	OutError.Reset();
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(InBlueprint);
	if (!Blueprint || !InWidget || !InClass)
	{
		OutError = TEXT("Blueprint, source widget and replacement class are required.");
		return false;
	}
	UWidgetTree* Tree = Blueprint->WidgetTree;
	UClass* SourceClass = InWidget->GetClass();
	if (Tree && Tree->FindWidget(InWidget->GetFName()) == InWidget &&
		(SourceClass->IsChildOf(InClass) || InClass->IsChildOf(SourceClass)))
	{
		if (InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			OutError = TEXT("The replacement must be a concrete current widget class.");
			return false;
		}
		const FName OriginalName = InWidget->GetFName();
		UUserWidget* ReplacementDefaults = Cast<UUserWidget>(InClass->GetDefaultObject());
		if (ReplacementDefaults && !Blueprint->IsWidgetFreeFromCircularReferences(ReplacementDefaults))
		{
			OutError = TEXT("The replacement would introduce a circular widget reference.");
			return false;
		}
		const FObjectPropertyBase* Binding = Blueprint->ParentClass ? FindFProperty<FObjectPropertyBase>(Blueprint->ParentClass, OriginalName) : nullptr;
		if (Binding && FWidgetBlueprintEditorUtils::IsBindWidgetProperty(Binding) && !InClass->IsChildOf(Binding->PropertyClass))
		{
			OutError = TEXT("The replacement does not satisfy the native BindWidget contract.");
			return false;
		}
		UPanelWidget* SourcePanel = Cast<UPanelWidget>(InWidget);
		if (SourcePanel && SourcePanel->HasAnyChildren() && !InClass->IsChildOf(UPanelWidget::StaticClass()))
		{
			OutError = TEXT("A widget with children requires a panel replacement.");
			return false;
		}
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				const UK2Node_ComponentBoundEvent* Event = Cast<UK2Node_ComponentBoundEvent>(Node);
				if (Event && Event->ComponentPropertyName == OriginalName && !InClass->IsChildOf(Event->DelegateOwnerClass))
				{
					OutError = TEXT("Retarget the component event before removing its declaring class.");
					return false;
				}
			}
		}
		FScopedTransaction Transaction(NSLOCTEXT("WidgetBlueprint", "ReplaceRelatedTemplate", "Replace related widget template"));
		Blueprint->Modify();
		Tree->Modify();
		InWidget->Modify();
		const bool bPreviouslySuppressed = Blueprint->bSuppressStructurallyModified;
		Blueprint->bSuppressStructurallyModified = true;
		ON_SCOPE_EXIT
		{
			Blueprint->bSuppressStructurallyModified = bPreviouslySuppressed;
		};
		UWidget* Replacement = NewObject<UWidget>(Tree, InClass, NAME_None, RF_Transactional);
		TMap<FName, FString> Properties;
		FWidgetBlueprintEditorUtils::ExportPropertiesToText(InWidget, Properties);
		FWidgetBlueprintEditorUtils::ImportPropertiesFromText(Replacement, Properties);
		bool bReplaced = false;
		if (TScriptInterface<INamedSlotInterface> Host = FWidgetBlueprintEditorUtils::FindNamedSlotHostForContent(InWidget, Tree))
		{
			bReplaced = FWidgetBlueprintEditorUtils::ReplaceNamedSlotHostContent(InWidget, Host, Replacement);
		}
		else if (UPanelWidget* Parent = InWidget->GetParent())
		{
			Parent->Modify();
			bReplaced = Parent->ReplaceChild(InWidget, Replacement);
		}
		else if (Tree->RootWidget == InWidget)
		{
			Tree->RootWidget = Replacement;
			bReplaced = true;
		}
		if (!bReplaced)
		{
			Replacement->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
			OutError = TEXT("The source widget could not be replaced in its parent.");
			return false;
		}
		InWidget->Slot = nullptr;
		if (SourcePanel && SourcePanel->HasAnyChildren())
		{
			UPanelWidget* ReplacementPanel = CastChecked<UPanelWidget>(Replacement);
			while (SourcePanel->HasAnyChildren())
			{
				UWidget* Child = SourcePanel->GetChildAt(0);
				Child->Modify();
				TMap<FName, FString> SlotProperties;
				FWidgetBlueprintEditorUtils::ExportPropertiesToText(Child->Slot, SlotProperties);
				UPanelSlot* NewSlot = ReplacementPanel->AddChild(Child);
				FWidgetBlueprintEditorUtils::ImportPropertiesFromText(NewSlot, SlotProperties);
			}
		}
		InWidget->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
		Replacement->Rename(*OriginalName.ToString(), Tree, REN_DontCreateRedirectors);
		Blueprint->MarkPackageDirty();
		return true;
	}
	FText Error;
	const bool bResult = FWidgetBlueprintOperationUtils::ReplaceWidgetWithTemplate(Blueprint, InWidget, InClass, Error);
	OutError = Error.ToString();
	return bResult;
}

bool UWidgetBlueprintEditorLibrary::RemoveWidget(UBlueprint* InBlueprint, UWidget* InWidget, FString& OutError)
{
	OutError.Reset();
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(InBlueprint);
	if (!Blueprint || !InWidget)
	{
		OutError = TEXT("Blueprint and source widget are required.");
		return false;
	}
	const bool bPreviouslySuppressed = Blueprint->bSuppressStructurallyModified;
	Blueprint->bSuppressStructurallyModified = true;
	ON_SCOPE_EXIT
	{
		Blueprint->bSuppressStructurallyModified = bPreviouslySuppressed;
	};
	FText Error;
	const bool bResult = FWidgetBlueprintOperationUtils::RemoveWidget(Blueprint, InWidget, Error);
	OutError = Error.ToString();
	return bResult;
}

bool UWidgetBlueprintEditorLibrary::RenameWidget(UBlueprint* InBlueprint, UWidget* InWidget, const FString& InName, FString& OutError)
{
	OutError.Reset();
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(InBlueprint);
	FText Error;
	if (!Blueprint || !InWidget || !FWidgetBlueprintOperationUtils::VerifyWidgetRename(Blueprint, InWidget, FText::FromString(InName), Error))
	{
		OutError = Error.IsEmpty() ? TEXT("Blueprint and source widget are required.") : Error.ToString();
		return false;
	}
	return FWidgetBlueprintOperationUtils::RenameWidget(Blueprint, InWidget, InName);
}

bool UWidgetBlueprintEditorLibrary::RetargetWidgetEvent(UBlueprint* InBlueprint, FName InComponentName, UClass* InDelegateOwnerClass, FName InDelegateName, FString& OutError)
{
	OutError.Reset();
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(InBlueprint);
	UClass* SourceClass = Blueprint ? Blueprint->SkeletonGeneratedClass.Get() : nullptr;
	const FObjectProperty* Component = SourceClass ? FindFProperty<FObjectProperty>(SourceClass, InComponentName) : nullptr;
	const FMulticastDelegateProperty* Delegate = InDelegateOwnerClass ? FindFProperty<FMulticastDelegateProperty>(InDelegateOwnerClass, InDelegateName) : nullptr;
	if (!Component || !Delegate || !Delegate->SignatureFunction)
	{
		OutError = TEXT("A source widget property and valid replacement delegate are required.");
		return false;
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	TArray<UK2Node_ComponentBoundEvent*> Events;
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_ComponentBoundEvent* Event = Cast<UK2Node_ComponentBoundEvent>(Node);
			if (Event && Event->ComponentPropertyName == InComponentName && Event->DelegatePropertyName == InDelegateName)
			{
				Events.Add(Event);
			}
		}
	}
	if (Events.Num() != 1)
	{
		OutError = TEXT("Exactly one matching component-bound event is required.");
		return false;
	}
	FScopedTransaction Transaction(NSLOCTEXT("WidgetBlueprint", "RetargetWidgetEvent", "Retarget widget event"));
	Blueprint->Modify();
	Events[0]->Modify();
	Events[0]->InitializeComponentBoundEventParams(Component, Delegate);
	Events[0]->ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return true;
}

bool UWidgetBlueprintEditorLibrary::ValidateWidgetIdentifiers(UBlueprint* InBlueprint, FString& OutError)
{
	OutError.Reset();
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(InBlueprint);
	if (!Blueprint)
	{
		OutError = TEXT("A Widget Blueprint is required.");
		return false;
	}
	TSet<FGuid> Seen;
	auto Validate = [Blueprint, &OutError, &Seen](FName Name)
	{
		const FGuid* Id = Blueprint->WidgetVariableNameToGuidMap.Find(Name);
		if (!Id || !Id->IsValid() || Seen.Contains(*Id))
		{
			OutError = FString::Printf(TEXT("Widget or animation %s has a missing, invalid or duplicate GUID in %s."), *Name.ToString(), *Blueprint->GetPathName());
			return false;
		}
		Seen.Add(*Id);
		return true;
	};
	for (UWidget* Widget : Blueprint->GetAllSourceWidgets())
	{
		if (!Validate(Widget->GetFName()))
		{
			return false;
		}
	}
	for (UWidgetAnimation* Animation : Blueprint->Animations)
	{
		if (Animation && !Validate(Animation->GetFName()))
		{
			return false;
		}
	}
	return true;
}

bool UWidgetBlueprintEditorLibrary::CompileWidgetBlueprint(UBlueprint* InBlueprint, FString& OutError)
{
	if (!ValidateWidgetIdentifiers(InBlueprint, OutError))
	{
		return false;
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InBlueprint);
	FBlueprintEditorUtils::RefreshAllNodes(InBlueprint);
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(InBlueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
	if (Results.NumErrors || InBlueprint->Status == BS_Error)
	{
		OutError = FString::Printf(TEXT("%s has %d compiler errors; inspect the Blueprint compiler log."), *InBlueprint->GetPathName(), Results.NumErrors);
		return false;
	}
	return true;
}
