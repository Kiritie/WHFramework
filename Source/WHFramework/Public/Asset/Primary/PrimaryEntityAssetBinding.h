#pragma once

#include "CoreMinimal.h"
#include "PrimaryEntityAssetBinding.generated.h"

class UPrimaryAssetBase;

USTRUCT()
struct WHFRAMEWORK_API FPrimaryEntityAssetBinding
{
	GENERATED_BODY()

	bool Bind(UObject* InOwner, const FPrimaryAssetId& InAssetId);
	void Reset();
	UPrimaryAssetBase* Get() const { return Asset; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UPrimaryAssetBase> Asset;
};
