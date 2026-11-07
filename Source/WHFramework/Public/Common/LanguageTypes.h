#pragma once

#include "CoreMinimal.h"

#include "LanguageTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FLanguageType
{
	GENERATED_BODY()

public:
	FORCEINLINE FLanguageType()
	{
		DisplayName = TEXT("");
		LocalCulture = TEXT("");
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString LocalCulture;
};
