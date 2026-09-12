#pragma once

#include "EventModuleTypes.h"

#include "Main/Base/ModuleBase.h"

#include "EventModule.generated.h"

class UEventManagerBase;

UCLASS()
class WHFRAMEWORK_API UEventModule : public UModuleBase
{
	GENERATED_BODY()

	GENERATED_MODULE(UEventModule)

public:
	UEventModule();
	~UEventModule();

#if WITH_EDITOR
	virtual void OnGenerate() override;
	virtual void OnDestroy() override;
#endif
	virtual void OnInitialize() override;
	virtual void OnPreparatory(EPhase InPhase) override;
	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;
	virtual void OnPause() override;
	virtual void OnUnPause() override;
	virtual void OnTermination(EPhase InPhase) override;

public:
	template<typename TEvent>
	FDelegateHandle SubscribeEvent(UObject* InOwner, TFunction<void(UObject*, const TEvent&)> InCallback)
	{
		static_assert(TIsDerivedFrom<TEvent, FEventBase>::Value, "Event must inherit FEventBase");

		FEventListener Listener;
		Listener.Owner = InOwner;
		Listener.Handle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
		Listener.Delegate.BindLambda([Callback = MoveTemp(InCallback)](UObject* InSender, FConstStructView InData)
		{
			Callback(InSender, InData.Get<TEvent>());
		});
		EventMappings.FindOrAdd(TEvent::StaticStruct()).Listeners.Add(MoveTemp(Listener));
		return EventMappings[TEvent::StaticStruct()].Listeners.Last().Handle;
	}

	template<typename TEvent, typename TObject>
	FDelegateHandle SubscribeEvent(TObject* InOwner, void (TObject::*InCallback)(UObject*, const TEvent&))
	{
		return SubscribeEvent<TEvent>(InOwner, [InOwner, InCallback](UObject* InSender, const TEvent& InEvent)
		{
			(InOwner->*InCallback)(InSender, InEvent);
		});
	}

	template<typename TEvent, typename TCallbackObject>
	FDelegateHandle SubscribeEvent(UObject* InOwner, void (TCallbackObject::*InCallback)())
	{
		return SubscribeEvent<TEvent>(InOwner, [InOwner, InCallback](UObject*, const TEvent&)
		{
			if(TCallbackObject* CallbackOwner = Cast<TCallbackObject>(InOwner))
			{
				(CallbackOwner->*InCallback)();
			}
		});
	}

	FDelegateHandle SubscribeEvent(UObject* InOwner, const UScriptStruct* InEventType, FEventDelegate InCallback);

	void UnsubscribeEvent(FDelegateHandle InHandle);

	template<typename TEvent>
	void UnsubscribeEvent(UObject* InOwner)
	{
		static_assert(TIsDerivedFrom<TEvent, FEventBase>::Value, "Event must inherit FEventBase");
		UnsubscribeEvent(TEvent::StaticStruct(), InOwner);
	}

	void UnsubscribeAllEvent();

	template<typename TEvent>
	void BroadcastEvent(UObject* InSender, const TEvent& InEvent = TEvent(), EEventNetType InNetType = EEventNetType::Local)
	{
		static_assert(TIsDerivedFrom<TEvent, FEventBase>::Value, "Event must inherit FEventBase");
		BroadcastEvent(InSender, FConstStructView::Make(InEvent), InNetType);
	}

	void BroadcastEvent(UObject* InSender, FConstStructView InEvent, EEventNetType InNetType = EEventNetType::Local);
	void BroadcastEventInternal(UObject* InSender, FConstStructView InEvent);

protected:
	void UnsubscribeEvent(const UScriptStruct* InEventType, UObject* InOwner);

	TMap<const UScriptStruct*, FEventMapping> EventMappings;

protected:
	UPROPERTY(EditAnywhere, Instanced)
	TArray<UEventManagerBase*> EventManagers;

	UPROPERTY(Transient)
	TMap<FName, UEventManagerBase*> EventManagerRefs;

public:
	template<class T>
	T* GetEventManager() const
	{
		return Cast<T>(GetEventManager(T::StaticClass()));
	}

	UFUNCTION(BlueprintPure)
	UEventManagerBase* GetEventManager(TSubclassOf<UEventManagerBase> InClass) const;

	UFUNCTION(BlueprintPure)
	UEventManagerBase* GetEventManagerByName(const FName InName, TSubclassOf<UEventManagerBase> InClass = nullptr) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
