#include "Parameter/K2Node/K2Node_GetParameter.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintFieldNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"
#include "Parameter/ParameterModuleStatics.h"
#include "Parameter/ParameterValueTypes.h"

#define LOCTEXT_NAMESPACE "K2Node_GetParameter"

namespace
{
	const FName ParameterPinName = TEXT("Parameter");
	const FName ValuePinName = TEXT("Value");
	const FName SuccessPinName = TEXT("Success");
}

void UK2Node_GetParameter::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, FParameter::StaticStruct(), ParameterPinName);
	if(ParameterStruct)
	{
		if(ParameterStruct->HasMetaData(TEXT("ParameterInlineValue")))
		{
			FEdGraphPinType PinType;
			if(const FProperty* ValueProperty = FindFProperty<FProperty>(ParameterStruct, ValuePinName);
				ValueProperty && GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(ValueProperty, PinType))
			{
				CreatePin(EGPD_Output, PinType, ValuePinName);
			}
		}
		else
		{
			CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, ParameterStruct, ValuePinName);
		}
	}
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Boolean, SuccessPinName);
}

void UK2Node_GetParameter::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
	if(!CanUseStruct(ParameterStruct, true))
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("MissingType", "@@ has no valid Parameter type.").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	UK2Node_CallFunction* CallNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallNode->SetFromFunction(UParameterModuleStatics::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UParameterModuleStatics, GetParameterValue)));
	CallNode->AllocateDefaultPins();
	UEdGraphPin* CallValuePin = CallNode->FindPinChecked(ValuePinName);
	CallValuePin->PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	CallValuePin->PinType.PinSubCategoryObject = ParameterStruct;
	CompilerContext.MovePinLinksToIntermediate(*GetParameterPin(), *CallNode->FindPinChecked(ParameterPinName));
	if(ParameterStruct->HasMetaData(TEXT("ParameterInlineValue")))
	{
		UK2Node_BreakStruct* BreakStructNode = CompilerContext.SpawnIntermediateNode<UK2Node_BreakStruct>(this, SourceGraph);
		BreakStructNode->StructType = ParameterStruct;
		BreakStructNode->bMadeAfterOverridePinRemoval = true;
		BreakStructNode->AllocateDefaultPins();
		CompilerContext.MessageLog.NotifyIntermediateObjectCreation(BreakStructNode, this);
		CallValuePin->MakeLinkTo(BreakStructNode->FindPinChecked(ParameterStruct->GetFName()));
		CompilerContext.MovePinLinksToIntermediate(*GetValuePin(), *BreakStructNode->FindPinChecked(ValuePinName));
	}
	else
	{
		CompilerContext.MovePinLinksToIntermediate(*GetValuePin(), *CallValuePin);
	}
	CompilerContext.MovePinLinksToIntermediate(*GetSuccessPin(), *CallNode->FindPinChecked(SuccessPinName));
	BreakAllNodeLinks();
}

FText UK2Node_GetParameter::GetNodeTitle(ENodeTitleType::Type) const
{
	return ParameterStruct ? FText::Format(LOCTEXT("TypedNodeTitle", "Get Parameter ({0})"), ParameterStruct->GetDisplayNameText()) : LOCTEXT("NodeTitle", "Get Parameter");
}

FText UK2Node_GetParameter::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Gets the selected Parameter value type and reports whether it matched.");
}

void UK2Node_GetParameter::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* NodeClass = GetClass();
	ActionRegistrar.RegisterStructActions(FBlueprintActionDatabaseRegistrar::FMakeStructSpawnerDelegate::CreateLambda([NodeClass](const UScriptStruct* Struct)
	{
		if(!CanUseStruct(Struct, false)) return static_cast<UBlueprintNodeSpawner*>(nullptr);
		UBlueprintFieldNodeSpawner* Spawner = UBlueprintFieldNodeSpawner::Create(NodeClass, const_cast<UScriptStruct*>(Struct));
		TWeakObjectPtr<UScriptStruct> WeakStruct = const_cast<UScriptStruct*>(Struct);
		Spawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda([WeakStruct](UEdGraphNode* NewNode, bool)
		{
			CastChecked<UK2Node_GetParameter>(NewNode)->ParameterStruct = WeakStruct.Get();
		});
		return static_cast<UBlueprintNodeSpawner*>(Spawner);
	}));
}

FText UK2Node_GetParameter::GetMenuCategory() const { return LOCTEXT("MenuCategory", "ParameterModule"); }

bool UK2Node_GetParameter::CanUseStruct(const UScriptStruct* Struct, bool bForInternalUse)
{
	return Struct && Struct != FParameterValueBase::StaticStruct() &&
		UEdGraphSchema_K2::IsAllowableBlueprintVariableType(Struct, bForInternalUse) &&
		!Struct->HasMetaData(TEXT("Abstract")) && !Struct->HasMetaData(TEXT("Deprecated")) &&
		!Struct->HasMetaData(TEXT("Hidden")) && !Struct->HasMetaData(TEXT("ParameterHidden"));
}

UEdGraphPin* UK2Node_GetParameter::GetParameterPin() const { return FindPin(ParameterPinName); }
UEdGraphPin* UK2Node_GetParameter::GetValuePin() const { return FindPin(ValuePinName); }
UEdGraphPin* UK2Node_GetParameter::GetSuccessPin() const { return FindPin(SuccessPinName); }

#undef LOCTEXT_NAMESPACE
