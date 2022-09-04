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

public:
	//////////////////////////////////////////////////////////////////////////
	// Rotator
	template<class U>
	FORCEINLINE static FRotator LerpRotator(const FRotator& A, const FRotator& B, const U& Alpha, bool bNormalized = true)
	{
		const FRotator Delta = B - A;
		return A + (bNormalized ? Delta.GetNormalized() : Delta) * Alpha;
	}

	//////////////////////////////////////////////////////////////////////////
	// Vector
	static FVector RotatorVector(const FVector& Vector, const FRotator& Rotator, bool bRound = false, bool bAbsolute = false);

	static FVector RotatorVector(const FVector& Vector, ERightAngle Angle, bool bRound = false, bool bAbsolute = false);

	//////////////////////////////////////////////////////////////////////////
	// RightAngle
	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static float RightAngleToFloat(ERightAngle InAngle);

	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static ERightAngle GetOffsetRightAngle(ERightAngle InAngle, int32 InOffset);

	//////////////////////////////////////////////////////////////////////////
	// Direction
	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static EDirection InvertDirection(EDirection InDirection);

	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static EDirection RotateDirection(EDirection InDirection, ERightAngle InAngle);

	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static FVector DirectionToVector(EDirection InDirection, ERightAngle InAngle = ERightAngle::RA_0);

	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static FIndex DirectionToIndex(EDirection InDirection, ERightAngle InAngle = ERightAngle::RA_0);

	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static FIndex GetAdjacentIndex(FIndex InIndex, EDirection InDirection, ERightAngle InAngle = ERightAngle::RA_0);
	
	//////////////////////////////////////////////////////////////////////////
	// Ease
	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static float EvaluateByCurve(UCurveFloat* InCurve, float InTime, float InDuration);
	
	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static float EvaluateByEaseType(EEaseType InEaseType, float InTime, float InDuration);

	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static float BounceEaseIn(float InTime, float InDuration);
	
	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static float BounceEaseInOut(float InTime, float InDuration);
	
	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static float BounceEaseOut(float InTime, float InDuration);

	//////////////////////////////////////////////////////////////////////////
	// Noise
	UFUNCTION(BlueprintPure, Category = "MathBPLibrary")
	static float GetNoiseHeight(FIndex InIndex, FVector InScale, int32 InOffset = 0);
};
