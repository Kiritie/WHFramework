#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "ObjectPoolModuleTypes.generated.h"

UENUM(BlueprintType)
enum class EObjectDespawnMode : uint8
{
	Recovery,

	Destroy
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSpawnParameter
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FObjectSpawnParameter : public FSpawnParameter
{
	GENERATED_BODY()

public:
	FObjectSpawnParameter() = default;

	explicit FObjectSpawnParameter(UObject* InOuter)
		: Outer(InOuter)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UObject> Outer = nullptr;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FActorSpawnParameter : public FSpawnParameter
{
	GENERATED_BODY()

public:
	FActorSpawnParameter() = default;

	explicit FActorSpawnParameter(
		UObject* InWorldContext,
		const FTransform& InTransform = FTransform::Identity,
		AActor* InOwner = nullptr)
		: WorldContext(InWorldContext)
		, Transform(InTransform)
		, Owner(InOwner)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UObject> WorldContext = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTransform Transform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> Owner = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<APawn> Instigator = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESpawnActorCollisionHandlingMethod CollisionHandling =
		ESpawnActorCollisionHandlingMethod::Undefined;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetSpawnParameter : public FSpawnParameter
{
	GENERATED_BODY()

public:
	FWidgetSpawnParameter() = default;

	explicit FWidgetSpawnParameter(UObject* InOwningObject)
		: OwningObject(InOwningObject)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UObject> OwningObject = nullptr;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FObjectPoolPolicy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnablePooling = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxIdle = -1;
};

class WHFRAMEWORK_API FObjectPoolSpawnParameterRegistry
{
public:
	static void Register(UClass* InClass, UScriptStruct* InStruct);

	static void Unregister(UClass* InClass);

	static void Clear();

	static UScriptStruct* Resolve(UClass* InClass);
};

class WHFRAMEWORK_API FObjectPoolPolicyRegistry
{
public:
	static void Register(UClass* InClass, const FObjectPoolPolicy& InPolicy);

	static void Unregister(UClass* InClass);

	static FObjectPoolPolicy Resolve(UClass* InClass);
};
