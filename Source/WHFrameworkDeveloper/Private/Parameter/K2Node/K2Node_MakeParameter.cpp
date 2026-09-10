#include "Parameter/K2Node/K2Node_MakeParameter.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintFieldNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"
#include "Parameter/ParameterModuleStatics.h"
#include "Parameter/ParameterValueTypes.h"

#define LOCTEXT_NAMESPACE "K2Node_MakeParameter"

namespace
{
	const FName MakeValuePinName = TEXT("Value");
	const FName MakeDescriptionPinName = TEXT("Description");
	const FName MakeParameterPinName = TEXT("Parameter");
}

void UK2Node_MakeParameter::AllocateDefaultPins()
{
	if(ParameterStruct)
	{
		CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, ParameterStruct, MakeValuePinName);
	}
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Text, MakeDescriptionPinName);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, FParameter::StaticStruct(), MakeParameterPinName);
}

void UK2Node_MakeParameter::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
	if(!CanUseStruct(ParameterStruct, true))
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("MissingType", "@@ has no valid Parameter type.").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	UK2Node_CallFunction* CallNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallNode->SetFromFunction(UParameterModuleStatics::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UParameterModuleStatics, MakeParameter)));
	CallNode->AllocateDefaultPins();
	UEdGraphPin* CallValuePin = CallNode->FindPinChecked(MakeValuePinName);
	CallValuePin->PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	CallValuePin->PinType.PinSubCategoryObject = ParameterStruct;
	CompilerContext.MovePinLinksToIntermediate(*GetValuePin(), *CallValuePin);
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(MakeDescriptionPinName), *CallNode->FindPinChecked(MakeDescriptionPinName));
	CompilerContext.MovePinLinksToIntermediate(*GetParameterPin(), *CallNode->GetReturnValuePin());
	BreakAllNodeLinks();
}

FText UK2Node_MakeParameter::GetNodeTitle(ENodeTitleType::Type) const
{
	return ParameterStruct ? FText::Format(LOCTEXT("TypedNodeTitle", "Make Parameter ({0})"), ParameterStruct->GetDisplayNameText()) : LOCTEXT("NodeTitle", "Make Parameter");
}

FText UK2Node_MakeParameter::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Creates a Parameter from the selected Parameter value type.");
}

void UK2Node_MakeParameter::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* NodeClass = GetClass();
	ActionRegistrar.RegisterStructActions(FBlueprintActionDatabaseRegistrar::FMakeStructSpawnerDelegate::CreateLambda([NodeClass](const UScriptStruct* Struct)
	{
		if(!CanUseStruct(Struct, false)) return static_cast<UBlueprintNodeSpawner*>(nullptr);
		UBlueprintFieldNodeSpawner* Spawner = UBlueprintFieldNodeSpawner::Create(NodeClass, const_cast<UScriptStruct*>(Struct));
		TWeakObjectPtr<UScriptStruct> WeakStruct = const_cast<UScriptStruct*>(Struct);
		Spawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda([WeakStruct](UEdGraphNode* NewNode, bool)
		{
			CastChecked<UK2Node_MakeParameter>(NewNode)->ParameterStruct = WeakStruct.Get();
		});
		return static_cast<UBlueprintNodeSpawner*>(Spawner);
	}));
}

FText UK2Node_MakeParameter::GetMenuCategory() const { return LOCTEXT("MenuCategory", "ParameterModule"); }

bool UK2Node_MakeParameter::CanUseStruct(const UScriptStruct* Struct, bool)
{
	return Struct && Struct != FParameterValueBase::StaticStruct() && Struct->IsChildOf(FParameterValueBase::StaticStruct()) &&
		!Struct->HasMetaData(TEXT("Abstract")) && !Struct->HasMetaData(TEXT("Deprecated")) &&
		!Struct->HasMetaData(TEXT("Hidden")) && !Struct->HasMetaData(TEXT("ParameterHidden"));
}

UEdGraphPin* UK2Node_MakeParameter::GetValuePin() const { return FindPin(MakeValuePinName); }
UEdGraphPin* UK2Node_MakeParameter::GetParameterPin() const { return FindPin(MakeParameterPinName); }

#undef LOCTEXT_NAMESPACE
