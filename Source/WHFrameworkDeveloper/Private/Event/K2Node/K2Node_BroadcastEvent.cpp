#include "Event/K2Node/K2Node_BroadcastEvent.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintFieldNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "Event/EventModuleStatics.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "GameFramework/PlayerController.h"
#include "Main/NetworkDispatchTypes.h"

#define LOCTEXT_NAMESPACE "K2Node_BroadcastEvent"

namespace
{
	const FName BroadcastSenderPinName = TEXT("Sender");
	const FName BroadcastWorldPinName = TEXT("WorldContextObject");
	const FName BroadcastEventDataPinName = TEXT("EventData");
	const FName BroadcastEventFunctionPinName = TEXT("Event");
	const FName BroadcastNetTypePinName = TEXT("NetType");
	const FName BroadcastControllerPinName = TEXT("Controller");
}

void UK2Node_BroadcastEvent::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UObject::StaticClass(), BroadcastSenderPinName);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UObject::StaticClass(), BroadcastWorldPinName);
	if(EventStruct)
	{
		CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, EventStruct, BroadcastEventDataPinName);
	}
	UEdGraphPin* NetTypePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, StaticEnum<EEventNetType>(), BroadcastNetTypePinName);
	NetTypePin->DefaultValue = StaticEnum<EEventNetType>()->GetNameStringByValue(static_cast<int64>(EEventNetType::Local));
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, APlayerController::StaticClass(), BroadcastControllerPinName);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, FNetworkDispatchResult::StaticStruct(), UEdGraphSchema_K2::PN_ReturnValue);
}

void UK2Node_BroadcastEvent::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
	if(!CanUseStruct(EventStruct) || !FindPin(UEdGraphSchema_K2::PN_Execute) || !FindPin(UEdGraphSchema_K2::PN_Then) ||
		!FindPin(BroadcastSenderPinName) || !FindPin(BroadcastEventDataPinName) || !FindPin(BroadcastNetTypePinName) ||
		!FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output))
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
	UEdGraphPin* WorldPin = FindPin(BroadcastWorldPinName);
	UEdGraphPin* CallWorldPin = CallNode->FindPinChecked(BroadcastWorldPinName);
	if (WorldPin && (WorldPin->LinkedTo.Num() > 0 || WorldPin->DefaultObject))
	{
		CompilerContext.MovePinLinksToIntermediate(*WorldPin, *CallWorldPin);
	}
	else
	{
		if (!GetBlueprint() || !FBlueprintEditorUtils::ImplementsGetWorld(GetBlueprint()))
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ExplicitWorldRequired", "@@ requires an explicit World context in this Blueprint.").ToString(), this);
			BreakAllNodeLinks();
			return;
		}
		UK2Node_Self* SelfNode = CompilerContext.SpawnIntermediateNode<UK2Node_Self>(this, SourceGraph);
		SelfNode->AllocateDefaultPins();
		if (!CompilerContext.GetSchema()->TryCreateConnection(SelfNode->FindPinChecked(UEdGraphSchema_K2::PN_Self), CallWorldPin))
			CompilerContext.MessageLog.Error(*LOCTEXT("MissingWorldContext", "Unable to connect World context for @@.").ToString(), this);
	}
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(BroadcastEventDataPinName), *CallEventPin);
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(BroadcastNetTypePinName), *CallNode->FindPinChecked(BroadcastNetTypePinName));
	if (UEdGraphPin* ControllerPin = FindPin(BroadcastControllerPinName))
		CompilerContext.MovePinLinksToIntermediate(*ControllerPin, *CallNode->FindPinChecked(BroadcastControllerPinName));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), *CallNode->GetReturnValuePin());
	BreakAllNodeLinks();
}

FText UK2Node_BroadcastEvent::GetNodeTitle(ENodeTitleType::Type) const
{
	return EventStruct ? FText::Format(LOCTEXT("TypedNodeTitle", "Broadcast Event ({0})"), EventStruct->GetDisplayNameText()) : LOCTEXT("NodeTitle", "Broadcast Event");
}

FText UK2Node_BroadcastEvent::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Broadcasts the selected Event and reports local delivery or RPC dispatch; remote execution is not implied.");
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
