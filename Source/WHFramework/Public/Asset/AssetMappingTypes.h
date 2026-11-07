#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "AssetMappingTypes.generated.h"

USTRUCT(BlueprintType)
struct FEnumMapping
{
	GENERATED_BODY()

public:
	FEnumMapping()
	{
		EnumNames = TArray<FString>();
	}

public:
	UPROPERTY(EditAnywhere)
	TArray<FString> EnumNames;
};

USTRUCT(BlueprintType)
struct FStaticClass
{
	GENERATED_BODY()

public:
	FStaticClass()
	{
		BaseClass = nullptr;
		ClassName = TEXT("");
		LoadedClass = nullptr;
	}

	FStaticClass(UClass* InBaseClass, const FString& InClassName) : FStaticClass()
	{
		BaseClass = InBaseClass;
		ClassName = InClassName;
	}

	FStaticClass(UClass* InBaseClass, const FSoftClassPath& InClassPath) : FStaticClass()
	{
		BaseClass = InBaseClass;
		ClassPath = InClassPath;
	}

public:
	FString GetClassName() const
	{
		return !ClassName.IsEmpty() ? ClassName : ClassPath.ToString();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UClass* BaseClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString ClassName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FSoftClassPath ClassPath;

	UPROPERTY(Transient)
	UClass* LoadedClass;
};

USTRUCT(BlueprintType)
struct FStaticObject
{
	GENERATED_BODY()

public:
	FStaticObject()
	{
		BaseClass = nullptr;
		ObjectName = TEXT("");
		LoadedObject = nullptr;
	}

	FStaticObject(UClass* InBaseClass, const FString& InObjectName) : FStaticObject()
	{
		BaseClass = InBaseClass;
		ObjectName = InObjectName;
	}

	FStaticObject(UClass* InBaseClass, const FSoftObjectPath& InObjectPath) : FStaticObject()
	{
		BaseClass = InBaseClass;
		ObjectPath = InObjectPath;
	}

public:
	FString GetObjectName() const
	{
		return !ObjectName.IsEmpty() ? ObjectName : ObjectPath.GetLongPackageName();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UClass* BaseClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString ObjectName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FSoftObjectPath ObjectPath;

	UPROPERTY(Transient)
	UObject* LoadedObject;
};
