#include "Event/K2Node/K2Node_SubscribeEvent.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintFieldNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "Event/EventModuleStatics.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "K2Node_SubscribeEvent"

namespace
{
	const FName SubscribeOwnerPinName = TEXT("Owner");
	const FName SubscribeEventStructPinName = TEXT("EventStruct");
	const FName SubscribeCallbackPinName = TEXT("Callback");
	const FName SubscribeSenderPinName = TEXT("Sender");
	const FName SubscribeEventDataPinName = TEXT("EventData");
	const FName SubscribePackedEventPinName = TEXT("Event");
}

void UK2Node_SubscribeEvent::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UObject::StaticClass(), SubscribeOwnerPinName);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, SubscribeCallbackPinName);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Object, UObject::StaticClass(), SubscribeSenderPinName);
	if(EventStruct)
	{
		CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, EventStruct, SubscribeEventDataPinName);
	}
}

void UK2Node_SubscribeEvent::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
	if(!CanUseStruct(EventStruct) || !FindPin(UEdGraphSchema_K2::PN_Execute) || !FindPin(UEdGraphSchema_K2::PN_Then) ||
		!FindPin(SubscribeOwnerPinName) || !FindPin(SubscribeCallbackPinName) || !FindPin(SubscribeSenderPinName) || !FindPin(SubscribeEventDataPinName))
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("MissingType", "@@ has no valid Event type.").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	bool bIsErrorFree = true;
	UK2Node_CallFunction* SubscribeNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	SubscribeNode->SetFromFunction(UEventModuleStatics::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UEventModuleStatics, K2_SubscribeEvent)));
	SubscribeNode->AllocateDefaultPins();
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(UEdGraphSchema_K2::PN_Execute), *SubscribeNode->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(UEdGraphSchema_K2::PN_Then), *SubscribeNode->GetThenPin());

	UEdGraphPin* OwnerPin = FindPinChecked(SubscribeOwnerPinName);
	UEdGraphPin* SubscribeOwnerPin = SubscribeNode->FindPinChecked(SubscribeOwnerPinName);
	if(OwnerPin->LinkedTo.Num() > 0 || OwnerPin->DefaultObject)
	{
		CompilerContext.MovePinLinksToIntermediate(*OwnerPin, *SubscribeOwnerPin);
	}
	else
	{
		if (!GetBlueprint() || !FBlueprintEditorUtils::ImplementsGetWorld(GetBlueprint()))
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ExplicitOwnerRequired", "@@ requires an Owner with a World context in this Blueprint.").ToString(), this);
			BreakAllNodeLinks();
			return;
		}
		UK2Node_Self* SelfNode = CompilerContext.SpawnIntermediateNode<UK2Node_Self>(this, SourceGraph);
		SelfNode->AllocateDefaultPins();
		bIsErrorFree &= Schema->TryCreateConnection(SelfNode->FindPinChecked(UEdGraphSchema_K2::PN_Self), SubscribeOwnerPin);
	}
	SubscribeNode->FindPinChecked(SubscribeEventStructPinName)->DefaultObject = EventStruct;

	const FDelegateProperty* CallbackProperty = FindFProperty<FDelegateProperty>(SubscribeNode->GetTargetFunction(), SubscribeCallbackPinName);
	UK2Node_CustomEvent* CallbackEventNode = CompilerContext.SpawnIntermediateNode<UK2Node_CustomEvent>(this, SourceGraph);
	CallbackEventNode->CustomFunctionName = *FString::Printf(TEXT("EventCallback_%s"), *CompilerContext.GetGuid(this));
	CallbackEventNode->AllocateDefaultPins();
	if(CallbackProperty && CallbackProperty->SignatureFunction)
	{
		for(TFieldIterator<FProperty> PropertyIt(CallbackProperty->SignatureFunction); PropertyIt && (PropertyIt->PropertyFlags & CPF_Parm); ++PropertyIt)
		{
			const FProperty* Parameter = *PropertyIt;
			if(!Parameter->HasAnyPropertyFlags(CPF_OutParm) || Parameter->HasAnyPropertyFlags(CPF_ReferenceParm))
			{
				FEdGraphPinType PinType;
				if (!Schema->ConvertPropertyToPinType(Parameter, PinType) ||
					!CallbackEventNode->CreateUserDefinedPin(Parameter->GetFName(), PinType, EGPD_Output))
				{
					bIsErrorFree = false;
					break;
				}
			}
		}
	}
	else
	{
		bIsErrorFree = false;
	}
	UEdGraphPin* CallbackDelegatePin = CallbackEventNode->FindPin(UK2Node_CustomEvent::DelegateOutputName);
	UEdGraphPin* CallbackSenderPin = CallbackEventNode->FindPin(SubscribeSenderPinName);
	UEdGraphPin* CallbackDataPin = CallbackEventNode->FindPin(SubscribeEventDataPinName);
	if (!bIsErrorFree || !CallbackDelegatePin || !CallbackSenderPin || !CallbackDataPin)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("InvalidCallbackSignature", "@@ cannot construct the Event callback signature.").ToString(), this);
		BreakAllNodeLinks();
		return;
	}
	bIsErrorFree &= Schema->TryCreateConnection(SubscribeNode->FindPinChecked(SubscribeCallbackPinName), CallbackDelegatePin);

	UK2Node_CallFunction* GetDataNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	GetDataNode->SetFromFunction(UEventModuleStatics::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UEventModuleStatics, K2_GetEventData)));
	GetDataNode->AllocateDefaultPins();
	UEdGraphPin* TypedEventDataPin = GetDataNode->FindPinChecked(SubscribeEventDataPinName);
	TypedEventDataPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	TypedEventDataPin->PinType.PinSubCategoryObject = EventStruct;
	bIsErrorFree &= Schema->TryCreateConnection(CallbackDataPin, GetDataNode->FindPinChecked(SubscribePackedEventPinName));

	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(SubscribeCallbackPinName), *CallbackEventNode->FindPinChecked(UEdGraphSchema_K2::PN_Then));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(SubscribeSenderPinName), *CallbackSenderPin);
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(SubscribeEventDataPinName), *TypedEventDataPin);
	if(!bIsErrorFree)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("InternalConnectionError", "Internal connection error in @@.").ToString(), this);
	}
	BreakAllNodeLinks();
}

FText UK2Node_SubscribeEvent::GetNodeTitle(ENodeTitleType::Type) const
{
	return EventStruct ? FText::Format(LOCTEXT("TypedNodeTitle", "Subscribe Event ({0})"), EventStruct->GetDisplayNameText()) : LOCTEXT("NodeTitle", "Subscribe Event");
}

FText UK2Node_SubscribeEvent::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Subscribes to the selected Event structure.");
}

void UK2Node_SubscribeEvent::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* NodeClass = GetClass();
	ActionRegistrar.RegisterStructActions(FBlueprintActionDatabaseRegistrar::FMakeStructSpawnerDelegate::CreateLambda([NodeClass](const UScriptStruct* Struct)
	{
		if(!CanUseStruct(Struct)) return static_cast<UBlueprintNodeSpawner*>(nullptr);
		UBlueprintFieldNodeSpawner* Spawner = UBlueprintFieldNodeSpawner::Create(NodeClass, const_cast<UScriptStruct*>(Struct));
		TWeakObjectPtr<UScriptStruct> WeakStruct = const_cast<UScriptStruct*>(Struct);
		Spawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda([WeakStruct](UEdGraphNode* NewNode, bool)
		{
			CastChecked<UK2Node_SubscribeEvent>(NewNode)->EventStruct = WeakStruct.Get();
		});
		return static_cast<UBlueprintNodeSpawner*>(Spawner);
	}));
}

FText UK2Node_SubscribeEvent::GetMenuCategory() const { return LOCTEXT("MenuCategory", "EventModule"); }

bool UK2Node_SubscribeEvent::CanUseStruct(const UScriptStruct* Struct)
{
	return Struct && Struct != FEventBase::StaticStruct() && Struct->IsChildOf(FEventBase::StaticStruct()) &&
		!Struct->HasMetaData(TEXT("Abstract")) && !Struct->HasMetaData(TEXT("Deprecated")) &&
		!Struct->HasMetaData(TEXT("Hidden")) && !Struct->HasMetaData(TEXT("EventHidden"));
}

#undef LOCTEXT_NAMESPACE
