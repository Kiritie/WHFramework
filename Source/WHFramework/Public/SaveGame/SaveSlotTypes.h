#pragma once

#include "CoreMinimal.h"

#include "SaveSlotTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSaveSlotHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGuid SaveId;

	bool IsValid() const
	{
		return SaveId.IsValid();
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCreateSaveSlotParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName InitialMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCaptureCurrentWorld = false;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSaveSlotSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGuid SaveId;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FString Description;

	UPROPERTY(BlueprintReadOnly)
	FDateTime CreatedAt;

	UPROPERTY(BlueprintReadOnly)
	FDateTime UpdatedAt;

	UPROPERTY(BlueprintReadOnly)
	FName CurrentMap;

	UPROPERTY(BlueprintReadOnly)
	double PlayTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly)
	FString PreviewPath;

	UPROPERTY(BlueprintReadOnly)
	bool bValid = false;
};
