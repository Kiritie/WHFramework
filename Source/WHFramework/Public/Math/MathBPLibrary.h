// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MathTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "MathBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UMathBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	//////////////////////////////////////////////////////////////////////////
	// Index
public:
	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static EDirection InvertDirection(EDirection InDirection);

	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static FVector DirectionToVector(EDirection InDirection, FRotator InRotation = FRotator::ZeroRotator);

	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static FIndex DirectionToIndex(EDirection InDirection, FRotator InRotation = FRotator::ZeroRotator);

	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static FIndex GetAdjacentIndex(FIndex InIndex, EDirection InDirection, FRotator InRotation = FRotator::ZeroRotator);
};
