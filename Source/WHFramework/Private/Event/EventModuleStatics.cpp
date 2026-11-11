#include "Event/EventModuleStatics.h"
#include "GameFramework/PlayerController.h"

void UEventModuleStatics::UnsubscribeAllEvent(const UObject* WorldContextObject)
{
	if (UEventModule* Events = UEventModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr)) Events->UnsubscribeAllEvent();
}

FNetworkDispatchResult UEventModuleStatics::K2_BroadcastEvent(const UObject*, UObject*, const int32&, EEventNetType, APlayerController*)
{
	checkNoEntry();
	return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
}

DEFINE_FUNCTION(UEventModuleStatics::execK2_BroadcastEvent)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_OBJECT(UObject, Sender);
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	const FStructProperty* EventProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	const void* EventPtr = Stack.MostRecentPropertyAddress;
	P_GET_ENUM(EEventNetType, NetType);
	P_GET_OBJECT(APlayerController, Controller);
	P_FINISH;
	P_NATIVE_BEGIN;
	FNetworkDispatchResult Result = FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
	if(EventProperty && EventPtr && EventProperty->Struct->IsChildOf(FEventBase::StaticStruct()))
	{
		if (UEventModule* Events = UEventModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr))
			Result = Events->BroadcastEvent(Sender, FConstStructView(EventProperty->Struct, static_cast<const uint8*>(EventPtr)), static_cast<EEventNetType>(NetType), Controller);
		else Result.Status = ENetworkDispatchStatus::ModuleUnavailable;
	}
	*static_cast<FNetworkDispatchResult*>(RESULT_PARAM) = MoveTemp(Result);
	P_NATIVE_END;
}

void UEventModuleStatics::K2_SubscribeEvent(UObject* Owner, UScriptStruct* EventStruct, FEventDynamicDelegate Callback)
{
	if (!::IsValid(Owner) || !EventStruct || !Callback.IsBound()) return;
	FEventDelegate NativeCallback;
	NativeCallback.BindLambda([Callback = MoveTemp(Callback)](UObject* Sender, FConstStructView Event) mutable
	{
		FInstancedStruct EventData(Event);
		Callback.ExecuteIfBound(Sender, EventData);
	});
	if (UEventModule* Events = UEventModule::GetPtr(Owner ? Owner->GetWorld() : nullptr)) Events->SubscribeEvent(Owner, EventStruct, MoveTemp(NativeCallback));
}

void UEventModuleStatics::K2_GetEventData(const FInstancedStruct&, int32&)
{
	checkNoEntry();
}

DEFINE_FUNCTION(UEventModuleStatics::execK2_GetEventData)
{
	P_GET_STRUCT_REF(FInstancedStruct, Event);
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	const FStructProperty* EventDataProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* EventDataPtr = Stack.MostRecentPropertyAddress;
	P_FINISH;
	P_NATIVE_BEGIN;
	if(EventDataProperty && EventDataPtr)
	{
		EventDataProperty->Struct->ClearScriptStruct(EventDataPtr);
		if(Event.IsValid() && Event.GetScriptStruct() == EventDataProperty->Struct)
		{
			EventDataProperty->Struct->CopyScriptStruct(EventDataPtr, Event.GetMemory());
		}
	}
	P_NATIVE_END;
}

UEventManagerBase* UEventModuleStatics::GetEventManager(const UObject* WorldContextObject, TSubclassOf<UEventManagerBase> InClass)
{
	UEventModule* Events = UEventModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Events ? Events->GetEventManager(InClass) : nullptr;
}

UEventManagerBase* UEventModuleStatics::GetEventManagerByName(const UObject* WorldContextObject, const FName InName, TSubclassOf<UEventManagerBase> InClass)
{
	UEventModule* Events = UEventModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Events ? Events->GetEventManagerByName(InName, InClass) : nullptr;
}
