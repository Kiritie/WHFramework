// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/CommonTypes.h"
#include "AssetBase.generated.h"

/**
 * Imported spreadsheet table.
 */
UCLASS(BlueprintType, Meta = (LoadBehavior = "LazyOnDemand"))
class WHFRAMEWORK_API UAssetBase : public UObject
{
	GENERATED_BODY()

public:
	UAssetBase(const FObjectInitializer& ObjectInitializer);

public:
	#if WITH_EDITORONLY_DATA
	virtual void Serialize(FStructuredArchiveRecord Record) override;
	#endif

	virtual void Initialize();

	virtual UAssetBase* Duplicate()
	{
		UAssetBase* Asset = DuplicateObject<UAssetBase>(this, GetMutableWorldContext());
		Asset->SourceObject = this;
		return Asset;
	}

	template<class T>
	T* Duplicate()
	{
		return Cast<T>(Duplicate());
	}

	//////////////////////////////////////////////////////////////////////////
	/// Asset Stats
public:
	/// 显示名称 
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(AssetRegistrySearchable)
	TSubclassOf<UAssetBase> NativeClass;

	UPROPERTY(BlueprintReadOnly)
	UAssetBase* SourceObject;
};
