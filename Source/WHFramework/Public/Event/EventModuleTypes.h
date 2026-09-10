#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"

#include "EventModuleTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventBase
{
	GENERATED_BODY()
};

USTRUCT()
struct WHFRAMEWORK_API FEventNetworkMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FInstancedStruct Event;
};

DECLARE_DELEGATE_TwoParams(FEventDelegate, UObject*, FConstStructView);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FEventDynamicDelegate, UObject*, Sender, const FInstancedStruct&, EventData);

UENUM(BlueprintType)
enum class EEventNetType : uint8
{
	Local,
	Server,
	Client,
	Multicast
};

struct WHFRAMEWORK_API FEventListener
{
	TWeakObjectPtr<UObject> Owner;
	FDelegateHandle Handle;
	FEventDelegate Delegate;
};

struct WHFRAMEWORK_API FEventMapping
{
	TArray<FEventListener> Listeners;
};
