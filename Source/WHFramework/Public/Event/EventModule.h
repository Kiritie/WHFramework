#pragma once

#include "Common/LifecycleTypes.h"
#include "Main/MainModuleAccess.h"
#include "EventModuleTypes.h"
#include "Main/NetworkDispatchTypes.h"

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
		if (!InCallback) return FDelegateHandle();

		FEventDelegate Delegate;
		Delegate.BindLambda([Callback = MoveTemp(InCallback)](UObject* InSender, FConstStructView InData)
		{
			Callback(InSender, InData.Get<TEvent>());
		});
		return SubscribeEvent(InOwner, TEvent::StaticStruct(), MoveTemp(Delegate));
	}

	template<typename TEvent, typename TObject>
	FDelegateHandle SubscribeEvent(TObject* InOwner, void (TObject::*InCallback)(UObject*, const TEvent&))
	{
		if (!InOwner || !InCallback) return FDelegateHandle();
		return SubscribeEvent<TEvent>(InOwner, [InOwner, InCallback](UObject* InSender, const TEvent& InEvent)
		{
			(InOwner->*InCallback)(InSender, InEvent);
		});
	}

	template<typename TEvent, typename TCallbackObject>
	FDelegateHandle SubscribeEvent(UObject* InOwner, void (TCallbackObject::*InCallback)())
	{
		if (!InOwner || !InCallback) return FDelegateHandle();
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
	FNetworkDispatchResult BroadcastEvent(UObject* InSender, const TEvent& InEvent = TEvent(), EEventNetType InNetType = EEventNetType::Local, APlayerController* InController = nullptr)
	{
		static_assert(TIsDerivedFrom<TEvent, FEventBase>::Value, "Event must inherit FEventBase");
		return BroadcastEvent(InSender, FConstStructView::Make(InEvent), InNetType, InController);
	}

	FNetworkDispatchResult BroadcastEvent(UObject* InSender, FConstStructView InEvent, EEventNetType InNetType = EEventNetType::Local, APlayerController* InController = nullptr);
	void BroadcastEventInternal(UObject* InSender, FConstStructView InEvent);
	virtual bool CanAcceptClientEvent(const APlayerController* InController, FConstStructView InEvent) const { return false; }

	FNetworkDispatchResult MulticastWorldEvent(const FEventNetworkMessage& InMessage);

protected:
	void UnsubscribeEvent(const UScriptStruct* InEventType, UObject* InOwner);

	TMap<const UScriptStruct*, FEventMapping> EventMappings;
	bool bEventDeliveryClosing = false;
	bool IsEventDeliveryClosing() const { return bEventDeliveryClosing || !IsBusinessAdmissionOpen(); }

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
