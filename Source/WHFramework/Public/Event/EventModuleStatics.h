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
		return UEventModule::Get().SubscribeEvent<TEvent>(InOwner, MoveTemp(InCallback));
	}

	template<typename TEvent, typename TObject>
	static FDelegateHandle SubscribeEvent(TObject* InOwner, void (TObject::*InCallback)(UObject*, const TEvent&))
	{
		return UEventModule::Get().SubscribeEvent<TEvent>(InOwner, InCallback);
	}

	template<typename TEvent, typename TCallbackObject>
	static FDelegateHandle SubscribeEvent(UObject* InOwner, void (TCallbackObject::*InCallback)())
	{
		return UEventModule::Get().SubscribeEvent<TEvent>(InOwner, InCallback);
	}

	static void UnsubscribeEvent(FDelegateHandle InHandle)
	{
		UEventModule::Get().UnsubscribeEvent(InHandle);
	}

	template<typename TEvent>
	static void UnsubscribeEvent(UObject* InOwner)
	{
		UEventModule::Get().UnsubscribeEvent<TEvent>(InOwner);
	}

	UFUNCTION(BlueprintCallable, Category = "EventModule")
	static void UnsubscribeAllEvent();

	template<typename TEvent>
	static void BroadcastEvent(UObject* InSender, const TEvent& InEvent = TEvent(), EEventNetType InNetType = EEventNetType::Local)
	{
		UEventModule::Get().BroadcastEvent<TEvent>(InSender, InEvent, InNetType);
	}

	UFUNCTION(BlueprintCallable, CustomThunk, meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "Event"), Category = "EventModule")
	static void K2_BroadcastEvent(UObject* Sender, const int32& Event, EEventNetType NetType = EEventNetType::Local);
	DECLARE_FUNCTION(execK2_BroadcastEvent);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"), Category = "EventModule")
	static void K2_SubscribeEvent(UObject* Owner, UScriptStruct* EventStruct, FEventDynamicDelegate Callback);

	UFUNCTION(BlueprintPure, CustomThunk, meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "EventData"), Category = "EventModule")
	static void K2_GetEventData(const FInstancedStruct& Event, int32& EventData);
	DECLARE_FUNCTION(execK2_GetEventData);

public:
	template<class T>
	static T* GetEventManager()
	{
		return Cast<T>(GetEventManager(T::StaticClass()));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "EventModule")
	static UEventManagerBase* GetEventManager(TSubclassOf<UEventManagerBase> InClass);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "EventModule")
	static UEventManagerBase* GetEventManagerByName(const FName InName, TSubclassOf<UEventManagerBase> InClass = nullptr);
};
