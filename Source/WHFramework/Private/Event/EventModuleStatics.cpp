#include "Event/EventModuleStatics.h"

void UEventModuleStatics::UnsubscribeAllEvent()
{
	UEventModule::Get().UnsubscribeAllEvent();
}

void UEventModuleStatics::K2_BroadcastEvent(UObject*, const int32&, EEventNetType)
{
	checkNoEntry();
}

DEFINE_FUNCTION(UEventModuleStatics::execK2_BroadcastEvent)
{
	P_GET_OBJECT(UObject, Sender);
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	const FStructProperty* EventProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	const void* EventPtr = Stack.MostRecentPropertyAddress;
	P_GET_ENUM(EEventNetType, NetType);
	P_FINISH;
	P_NATIVE_BEGIN;
	if(EventProperty && EventPtr && EventProperty->Struct->IsChildOf(FEventBase::StaticStruct()))
	{
		UEventModule::Get().BroadcastEvent(Sender, FConstStructView(EventProperty->Struct, static_cast<const uint8*>(EventPtr)), static_cast<EEventNetType>(NetType));
	}
	P_NATIVE_END;
}

void UEventModuleStatics::K2_SubscribeEvent(UObject* Owner, UScriptStruct* EventStruct, FEventDynamicDelegate Callback)
{
	FEventDelegate NativeCallback;
	NativeCallback.BindLambda([Callback = MoveTemp(Callback)](UObject* Sender, FConstStructView Event) mutable
	{
		FInstancedStruct EventData(Event);
		Callback.ExecuteIfBound(Sender, EventData);
	});
	UEventModule::Get().SubscribeEvent(Owner, EventStruct, MoveTemp(NativeCallback));
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

UEventManagerBase* UEventModuleStatics::GetEventManager(TSubclassOf<UEventManagerBase> InClass)
{
	return UEventModule::Get().GetEventManager(InClass);
}

UEventManagerBase* UEventModuleStatics::GetEventManagerByName(const FName InName, TSubclassOf<UEventManagerBase> InClass)
{
	return UEventModule::Get().GetEventManagerByName(InName, InClass);
}
