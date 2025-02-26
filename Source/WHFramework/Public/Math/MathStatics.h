// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Math/MathTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "MathStatics.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UMathStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	// Index
	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static FIndex RotateIndex(const FIndex& InIndex, const FRotator& InRotator, bool bRound = false, bool bAbsolute = false);

	//////////////////////////////////////////////////////////////////////////
	// Vector
	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static FVector RotateVector(const FVector& InVector, const FRotator& InRotator, bool bRound = false, bool bAbsolute = false);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static bool IsPointInEllipse2D(FVector2D InPoint, FVector2D InCenter, FVector2D InRadius);

	//////////////////////////////////////////////////////////////////////////
	// RightAngle
	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static float RightAngleToFloat(ERightAngle InAngle);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static ERightAngle FloatToRightAngle(float InAngle);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static ERightAngle GetOffsetRightAngle(ERightAngle InAngle, int32 InOffset);

	//////////////////////////////////////////////////////////////////////////
	// Direction
	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static EDirection InvertDirection(EDirection InDirection);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static EDirection RotateDirection(EDirection InDirection, ERightAngle InAngle);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static FVector DirectionToVector(EDirection InDirection, ERightAngle InAngle = ERightAngle::RA_0);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static FIndex DirectionToIndex(EDirection InDirection, ERightAngle InAngle = ERightAngle::RA_0);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static FIndex GetAdjacentIndex(FIndex InIndex, EDirection InDirection, ERightAngle InAngle = ERightAngle::RA_0);
	
	//////////////////////////////////////////////////////////////////////////
	// Ease
	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static float EvaluateByCurve(UCurveFloat* InCurve, float InTime, float InDuration);
	
	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static float EvaluateByEaseType(EEaseType InEaseType, float InTime, float InDuration);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static float BounceEaseIn(float InTime, float InDuration);
	
	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static float BounceEaseInOut(float InTime, float InDuration);
	
	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static float BounceEaseOut(float InTime, float InDuration);

	//////////////////////////////////////////////////////////////////////////
	// Noise
	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static float GetNoise1D(float InValue, int32 InOffset = 0, bool bAbs = false, bool bUnsigned = false);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static float GetNoise2D(FVector2D InLocation, int32 InOffset = 0, bool bAbs = false, bool bUnsigned = false);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static float GetNoise3D(FVector InLocation, int32 InOffset = 0, bool bAbs = false, bool bUnsigned = false);
};
