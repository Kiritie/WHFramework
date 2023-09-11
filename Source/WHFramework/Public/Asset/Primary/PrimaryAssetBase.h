#pragma once

#include "Asset/AssetModuleTypes.h"

#include "PrimaryAssetBase.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UPrimaryAssetBase : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPrimaryAssetBase();

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void ResetData();
	
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FPrimaryAssetType Type;

public:
	virtual bool IsValid() const;
	
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	virtual bool EqualID(const FPrimaryAssetId& InAssetID) const;

public:
	template<class T>
	T* Cast()
	{
		return static_cast<T*>(this);
	}

	template<class T>
	T& CastRef()
	{
		return *Cast<T>();
	}
};
