#include "Event/K2Node/K2Node_BroadcastEvent.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintFieldNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "Event/EventModuleStatics.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"

#define LOCTEXT_NAMESPACE "K2Node_BroadcastEvent"

namespace
{
	const FName BroadcastSenderPinName = TEXT("Sender");
	const FName BroadcastEventDataPinName = TEXT("EventData");
	const FName BroadcastEventFunctionPinName = TEXT("Event");
	const FName BroadcastNetTypePinName = TEXT("NetType");
}

void UK2Node_BroadcastEvent::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UObject::StaticClass(), BroadcastSenderPinName);
	if(EventStruct)
	{
		CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, EventStruct, BroadcastEventDataPinName);
	}
	UEdGraphPin* NetTypePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, StaticEnum<EEventNetType>(), BroadcastNetTypePinName);
	NetTypePin->DefaultValue = StaticEnum<EEventNetType>()->GetNameStringByValue(static_cast<int64>(EEventNetType::Local));
}

void UK2Node_BroadcastEvent::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
	if(!CanUseStruct(EventStruct))
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("MissingType", "@@ has no valid Event type.").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	UK2Node_CallFunction* CallNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallNode->SetFromFunction(UEventModuleStatics::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UEventModuleStatics, K2_BroadcastEvent)));
	CallNode->AllocateDefaultPins();
	UEdGraphPin* CallEventPin = CallNode->FindPinChecked(BroadcastEventFunctionPinName);
	CallEventPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	CallEventPin->PinType.PinSubCategoryObject = EventStruct;

	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(UEdGraphSchema_K2::PN_Execute), *CallNode->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(UEdGraphSchema_K2::PN_Then), *CallNode->GetThenPin());
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(BroadcastSenderPinName), *CallNode->FindPinChecked(BroadcastSenderPinName));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(BroadcastEventDataPinName), *CallEventPin);
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(BroadcastNetTypePinName), *CallNode->FindPinChecked(BroadcastNetTypePinName));
	BreakAllNodeLinks();
}

FText UK2Node_BroadcastEvent::GetNodeTitle(ENodeTitleType::Type) const
{
	return EventStruct ? FText::Format(LOCTEXT("TypedNodeTitle", "Broadcast Event ({0})"), EventStruct->GetDisplayNameText()) : LOCTEXT("NodeTitle", "Broadcast Event");
}

FText UK2Node_BroadcastEvent::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Broadcasts the selected Event structure.");
}

void UK2Node_BroadcastEvent::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* NodeClass = GetClass();
	ActionRegistrar.RegisterStructActions(FBlueprintActionDatabaseRegistrar::FMakeStructSpawnerDelegate::CreateLambda([NodeClass](const UScriptStruct* Struct)
	{
		if(!CanUseStruct(Struct)) return static_cast<UBlueprintNodeSpawner*>(nullptr);
		UBlueprintFieldNodeSpawner* Spawner = UBlueprintFieldNodeSpawner::Create(NodeClass, const_cast<UScriptStruct*>(Struct));
		TWeakObjectPtr<UScriptStruct> WeakStruct = const_cast<UScriptStruct*>(Struct);
		Spawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda([WeakStruct](UEdGraphNode* NewNode, bool)
		{
			CastChecked<UK2Node_BroadcastEvent>(NewNode)->EventStruct = WeakStruct.Get();
		});
		return static_cast<UBlueprintNodeSpawner*>(Spawner);
	}));
}

FText UK2Node_BroadcastEvent::GetMenuCategory() const { return LOCTEXT("MenuCategory", "EventModule"); }

bool UK2Node_BroadcastEvent::CanUseStruct(const UScriptStruct* Struct)
{
	return Struct && Struct != FEventBase::StaticStruct() && Struct->IsChildOf(FEventBase::StaticStruct()) &&
		!Struct->HasMetaData(TEXT("Abstract")) && !Struct->HasMetaData(TEXT("Deprecated")) &&
		!Struct->HasMetaData(TEXT("Hidden")) && !Struct->HasMetaData(TEXT("EventHidden"));
}

#undef LOCTEXT_NAMESPACE
