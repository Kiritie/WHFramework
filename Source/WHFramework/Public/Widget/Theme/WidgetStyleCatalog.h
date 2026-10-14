#pragma once
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WidgetStyleCatalog.generated.h"

UENUM(BlueprintType)
enum class EWidgetStyleKind : uint8
{
	None,
	Button,
	Text,
	Brush
};

UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetStyleCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UWidgetStyleCatalog();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	TMap<FGameplayTag, EWidgetStyleKind> Types;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	TMap<EWidgetStyleKind, TSubclassOf<UObject>> NativeRoots;
};
