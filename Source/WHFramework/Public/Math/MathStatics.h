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
	static int64 CompressIndex(FIndex InIndex);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static FIndex UnCompressIndex(int64 InIndex);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static FIndex RotateIndex(const FIndex& InIndex, ERightAngle InAngle, bool bAbsolute = false);

	//////////////////////////////////////////////////////////////////////////
	// Vector
	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static FVector RotateVector(const FVector& InVector, const FRotator& InRotator, bool bRound = false, bool bAbsolute = false);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static bool IsPointInBox2D(const FVector2D& InPoint, const FVector2D& InCenter, const FVector2D& InRadius);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static bool IsPointInEllipse2D(const FVector2D& InPoint, const FVector2D& InCenter, const FVector2D& InRadius);
	
	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static bool IsPointInPolygon2D(const FVector2D& InPoint, const TArray<FVector2D>& InPoints);

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
	static EDirectionN InvertDirection(EDirectionN InDirection);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static EDirectionN RotateDirection(EDirectionN InDirection, ERightAngle InAngle);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static FVector DirectionToVector(EDirectionN InDirection, ERightAngle InAngle = ERightAngle::RA_0);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static FIndex DirectionToIndex(EDirectionN InDirection, ERightAngle InAngle = ERightAngle::RA_0);

	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static FIndex GetAdjacentIndex(FIndex InIndex, EDirectionN InDirection, ERightAngle InAngle = ERightAngle::RA_0);
	
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
