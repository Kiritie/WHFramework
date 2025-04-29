// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MathTypes.h"

/**
 * 
 */
class WHFRAMEWORKCORE_API FMathHelper
{
public:
	//////////////////////////////////////////////////////////////////////////
	// Index
	//Three-dimensional to one-dimensional coordinates (compressed)
	static uint64 Index(int32 InX, int32 InY, int32 InZ = 0);

	//Three-dimensional to one-dimensional coordinates (compressed)
	static uint64 Index(FIndex InIndex);

	//One-dimensional to three-dimensional coordinates (decompression coordinates)
	static FIndex UnIndex(uint64 InIndex);

	static FIndex RotateIndex(const FIndex& InIndex, ERightAngle InAngle, bool bAbsolute = false);

	//////////////////////////////////////////////////////////////////////////
	// Rotator
	template<class U>
	FORCEINLINE static FRotator LerpRotator(const FRotator& InA, const FRotator& InB, const U& InAlpha, bool bNormalized = true)
	{
		const FRotator Delta = InB - InA;
		return InA + (bNormalized ? Delta.GetNormalized() : Delta) * InAlpha;
	}

	//////////////////////////////////////////////////////////////////////////
	// Vector
	static FVector RotateVector(const FVector& InVector, const FRotator& InRotator, bool bRound = false, bool bAbsolute = false);

	static FVector RotateVector(const FVector& InVector, ERightAngle InAngle, bool bRound = false, bool bAbsolute = false);

	static bool IsPointInBox2D(const FVector2D& InPoint, const FVector2D& InCenter, const FVector2D& InRadius);

	static bool IsPointInEllipse2D(const FVector2D& InPoint, const FVector2D& InCenter, const FVector2D& InRadius);

	static bool IsPointInPolygon2D(const FVector2D& InPoint, const TArray<FVector2D>& InPoints);

	//////////////////////////////////////////////////////////////////////////
	// RightAngle
	static float RightAngleToFloat(ERightAngle InAngle);

	static ERightAngle FloatToRightAngle(float InAngle);

	static ERightAngle OffsetRightAngle(ERightAngle InAngle, int32 InOffset);

	static ERightAngle CombineRightAngle(ERightAngle InAngle1, ERightAngle InAngle2);

	//////////////////////////////////////////////////////////////////////////
	// Direction
	static EDirection InvertDirection(EDirection InDirection);

	static EDirection RotateDirection(EDirection InDirection, ERightAngle InAngle);

	static FVector DirectionToVector(EDirection InDirection, ERightAngle InAngle = ERightAngle::RA_0);

	static FIndex DirectionToIndex(EDirection InDirection, ERightAngle InAngle = ERightAngle::RA_0);

	static FIndex GetAdjacentIndex(FIndex InIndex, EDirection InDirection, ERightAngle InAngle = ERightAngle::RA_0);
	
	//////////////////////////////////////////////////////////////////////////
	// Ease
	static float EvaluateByCurve(UCurveFloat* InCurve, float InTime, float InDuration);
	
	static float EvaluateByEaseType(EEaseType InEaseType, float InTime, float InDuration);

	static float BounceEaseIn(float InTime, float InDuration);
	
	static float BounceEaseInOut(float InTime, float InDuration);
	
	static float BounceEaseOut(float InTime, float InDuration);

	//////////////////////////////////////////////////////////////////////////
	// Noise
	static float GetNoise1D(float InValue, int32 InOffset = 0, bool bAbs = false, bool bUnsigned = false);

	static float GetNoise2D(FVector2D InLocation, int32 InOffset = 0, bool bAbs = false, bool bUnsigned = false);

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
	static int32 HashRandInt(FVector2D InLocation, int32 InSeed = 0);
	
	//Return random value 0.0f~1.0f
	static float HashRand(FVector2D InLocation, int32 InSeed = 0);

	//Return random int value MinValue~MaxValue
	static int32 HashRandRange(FVector2D InLocation, int32 InMinValue, int32 InMaxValue, int32 InSeed = 0);

	//Return random float value MinValue~MaxValue
	static float HashRandRange(FVector2D InLocation, float InMinValue, float InMaxValue, int32 InSeed = 0);

	//Second order bezier curve t should be [0.0f~1.0f]
	static FVector2D Bezier(FVector2D InP0, FVector2D InP1, FVector2D InP2, float InT);

	//The third-order bezier curve t should be [0.0f~1.0f]
	static FVector2D Bezier(FVector2D InP0, FVector2D InP1, FVector2D InP2, FVector2D InP3, float InT);

protected:
	static int32 HashRandHelper(FVector2D InLocation, int32 InValue, int32 InSeed = 0);
};
