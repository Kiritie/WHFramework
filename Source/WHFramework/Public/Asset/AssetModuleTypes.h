#pragma once

#include "Engine/DataTable.h"
#include "AssetModuleTypes.generated.h"

class UPrimaryAssetBase;
/**
 *
 */
USTRUCT(BlueprintType)
struct FPrimaryAssets
{
	GENERATED_BODY()

public:
	FPrimaryAssets()
	{
		Assets = TMap<FPrimaryAssetId, UPrimaryAssetBase*>();
	}

	FPrimaryAssets(const TMap<FPrimaryAssetId, UPrimaryAssetBase*>& InAssets)
	{
		Assets = InAssets;
	}

public:
	UPROPERTY()
	TMap<FPrimaryAssetId, UPrimaryAssetBase*> Assets;
};

/**
 * 数据表项
 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FDataTableRowBase : public FTableRowBase
{
	GENERATED_BODY()

public:
	FDataTableRowBase()
	{
		ID = NAME_None;
	}

public:
	UPROPERTY(BlueprintReadOnly)
	FName ID;

public:
	virtual bool IsValid() const
	{
		return !ID.IsNone();
	}

	virtual void OnInitializeRow(const FName& InRowName);
};

/**
 *
 */
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

/**
 *
 */
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

/**
 *
 */
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
