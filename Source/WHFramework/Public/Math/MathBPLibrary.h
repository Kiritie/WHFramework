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
	// Lerp
public:
	template<class U>
	FORCEINLINE static FRotator Lerp(const FRotator& A, const FRotator& B, const U& Alpha, bool bNormalized = true)
	{
		const FRotator Delta = B - A;
		return A + (bNormalized ? Delta.GetNormalized() : Delta) * Alpha;
	}

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
	
	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static float EvaluateByCurve(UCurveFloat* InCurve, float InTime, float InDuration);
	
	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static float EvaluateByEaseType(EEaseType InEaseType, float InTime, float InDuration);

	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static float BounceEaseIn(float InTime, float InDuration, float IUnusedOvershootOrAmplitude, float InUnusedPeriod);
	
	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static float BounceEaseInOut(float InTime, float InDuration, float InUnusedOvershootOrAmplitude, float InUnusedPeriod);
	
	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static float BounceEaseOut(float InTime, float InDuration, float InUnusedOvershootOrAmplitude, float InUnusedPeriod);
};
