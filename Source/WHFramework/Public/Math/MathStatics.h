// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MathTypes.h"

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
	//Three-dimensional to one-dimensional coordinates (compressed)
	static uint64 Index(int32 InX, int32 InY, int32 InZ = 0);

	//Three-dimensional to one-dimensional coordinates (compressed)
	static uint64 Index(FIndex InIndex);

	//One-dimensional to three-dimensional coordinates (decompression coordinates)
	static FIndex UnIndex(uint64 InIndex);

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

	static bool IsPointInEllipse2D(FVector2D InPoint, FVector2D InCenter, FVector2D InRadius);

	//////////////////////////////////////////////////////////////////////////
	// RightAngle
	UFUNCTION(BlueprintPure, Category = "MathStatics")
	static float RightAngleToFloat(ERightAngle InAngle);

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

	//////////////////////////////////////////////////////////////////////////
	// Misc
	static int32 Hash11(int32 InValue);

	static int32 Hash11WithSeed(int32 InValue, int32 InSeed = 0);

	static FVector2D Hash22(FVector2D InLocation);

	static int32 Hash21(FVector2D InLocation);

	static FVector Hash33(FVector InLocation);

	static int32 Hash31(FVector InLocation);

	//Return random values 0~1023
	static int32 RandInt(FVector2D InLocation, int32 InSeed = 0);
	
	//Return random value 0.0f~1.0f
	static float Rand(FVector2D InLocation, int32 InSeed = 0);

	//Second order bezier curve t should be [0.0f~1.0f]
	static FVector2D Bezier(FVector2D InP0, FVector2D InP1, FVector2D InP2, float InT);

	//The third-order bezier curve t should be [0.0f~1.0f]
	static FVector2D Bezier(FVector2D InP0, FVector2D InP1, FVector2D InP2, FVector2D InP3, float InT);
};
