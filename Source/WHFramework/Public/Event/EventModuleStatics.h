#pragma once

#include "EventModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "EventModuleStatics.generated.h"

class UEventManagerBase;

UCLASS()
class WHFRAMEWORK_API UEventModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	template<typename TEvent>
	static FDelegateHandle SubscribeEvent(UObject* InOwner, TFunction<void(UObject*, const TEvent&)> InCallback)
	{
		if (UEventModule* Events = UEventModule::GetPtr(InOwner ? InOwner->GetWorld() : nullptr))
			return Events->SubscribeEvent<TEvent>(InOwner, MoveTemp(InCallback));
		return FDelegateHandle();
	}

	template<typename TEvent, typename TObject>
	static FDelegateHandle SubscribeEvent(TObject* InOwner, void (TObject::*InCallback)(UObject*, const TEvent&))
	{
		if (UEventModule* Events = UEventModule::GetPtr(InOwner ? InOwner->GetWorld() : nullptr))
			return Events->SubscribeEvent<TEvent>(InOwner, InCallback);
		return FDelegateHandle();
	}

	template<typename TEvent, typename TCallbackObject>
	static FDelegateHandle SubscribeEvent(UObject* InOwner, void (TCallbackObject::*InCallback)())
	{
		if (UEventModule* Events = UEventModule::GetPtr(InOwner ? InOwner->GetWorld() : nullptr))
			return Events->SubscribeEvent<TEvent>(InOwner, InCallback);
		return FDelegateHandle();
	}

	static void UnsubscribeEvent(const UObject* WorldContextObject, FDelegateHandle InHandle)
	{
		if (UEventModule* Events = UEventModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr)) Events->UnsubscribeEvent(InHandle);
	}

	template<typename TEvent>
	static void UnsubscribeEvent(UObject* InOwner)
	{
		if (UEventModule* Events = UEventModule::GetPtr(InOwner ? InOwner->GetWorld() : nullptr)) Events->UnsubscribeEvent<TEvent>(InOwner);
	}

	UFUNCTION(BlueprintCallable, Category = "EventModule", meta = (WorldContext = "WorldContextObject"))
	static void UnsubscribeAllEvent(const UObject* WorldContextObject);

	template<typename TEvent>
	static FNetworkDispatchResult BroadcastEvent(UObject* InSender, const TEvent& InEvent = TEvent(), EEventNetType InNetType = EEventNetType::Local, APlayerController* InController = nullptr)
	{
		if (UEventModule* Events = UEventModule::GetPtr(InSender ? InSender->GetWorld() : nullptr)) return Events->BroadcastEvent<TEvent>(InSender, InEvent, InNetType, InController);
		return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::ModuleUnavailable);
	}

	UFUNCTION(BlueprintCallable, CustomThunk, meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true", CustomStructureParam = "Event"), Category = "EventModule")
	static FNetworkDispatchResult K2_BroadcastEvent(const UObject* WorldContextObject, UObject* Sender, const int32& Event, EEventNetType NetType = EEventNetType::Local, APlayerController* Controller = nullptr);
	DECLARE_FUNCTION(execK2_BroadcastEvent);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"), Category = "EventModule")
	static void K2_SubscribeEvent(UObject* Owner, UScriptStruct* EventStruct, FEventDynamicDelegate Callback);

	UFUNCTION(BlueprintPure, CustomThunk, meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "EventData"), Category = "EventModule")
	static void K2_GetEventData(const FInstancedStruct& Event, int32& EventData);
	DECLARE_FUNCTION(execK2_GetEventData);

public:
	template<class T>
	static T* GetEventManager(const UObject* WorldContextObject)
	{
		return Cast<T>(GetEventManager(WorldContextObject, T::StaticClass()));
	}

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "EventModule")
	static UEventManagerBase* GetEventManager(const UObject* WorldContextObject, TSubclassOf<UEventManagerBase> InClass);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "EventModule")
	static UEventManagerBase* GetEventManagerByName(const UObject* WorldContextObject, const FName InName, TSubclassOf<UEventManagerBase> InClass = nullptr);
};
