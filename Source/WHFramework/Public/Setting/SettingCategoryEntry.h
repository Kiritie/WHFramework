#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "SettingCategoryEntry.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API USettingCategoryEntry : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(FName InCategory, const FText& InDisplayName, int32 InOrder);

	UFUNCTION(BlueprintPure)
	FName GetCategory() const { return Category; }

	UFUNCTION(BlueprintPure)
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure)
	int32 GetOrder() const { return Order; }

private:
	UPROPERTY()
	FName Category;

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	int32 Order = 0;
};
