// Fill out your copyright notice in the Description page of Project Settings.

#include "Math/MathStatics.h"

#include "Math/MathHelper.h"

FIndex UMathStatics::RotateIndex(const FIndex& InIndex, const FRotator& InRotator, bool bRound, bool bAbsolute)
{
	return RotateVector(InIndex, InRotator, bRound, bAbsolute);
}

FVector UMathStatics::RotateVector(const FVector& InVector, const FRotator& InRotator, bool bRound, bool bAbsolute)
{
	return FMathHelper::RotateVector(InVector, InRotator, bRound, bAbsolute);
}

bool UMathStatics::IsPointInEllipse2D(FVector2D InPoint, FVector2D InCenter, FVector2D InRadius)
{
	return FMathHelper::IsPointInEllipse2D(InPoint, InCenter, InRadius);
}

float UMathStatics::RightAngleToFloat(ERightAngle InAngle)
{
	return FMathHelper::RightAngleToFloat(InAngle);
}

ERightAngle UMathStatics::FloatToRightAngle(float InAngle)
{
	return FMathHelper::FloatToRightAngle(InAngle);
}

ERightAngle UMathStatics::GetOffsetRightAngle(ERightAngle InAngle, int32 InOffset)
{
	return FMathHelper::GetOffsetRightAngle(InAngle, InOffset);
}

EDirection UMathStatics::InvertDirection(EDirection InDirection)
{
	return FMathHelper::InvertDirection(InDirection);
}

EDirection UMathStatics::RotateDirection(EDirection InDirection, ERightAngle InAngle)
{
	return FMathHelper::RotateDirection(InDirection, InAngle);
}

FVector UMathStatics::DirectionToVector(EDirection InDirection, ERightAngle InAngle)
{
	return FMathHelper::DirectionToVector(InDirection, InAngle);
}

FIndex UMathStatics::DirectionToIndex(EDirection InDirection, ERightAngle InAngle)
{
	return FMathHelper::DirectionToIndex(InDirection, InAngle);
}

FIndex UMathStatics::GetAdjacentIndex(FIndex InIndex, EDirection InDirection, ERightAngle InAngle)
{
	return FMathHelper::GetAdjacentIndex(InIndex, InDirection, InAngle);
}

float UMathStatics::EvaluateByCurve(UCurveFloat* InCurve, float InTime, float InDuration)
{
	return FMathHelper::EvaluateByCurve(InCurve, InTime, InDuration);
}
	
float UMathStatics::EvaluateByEaseType(EEaseType InEaseType, float InTime, float InDuration)
{
	return FMathHelper::EvaluateByEaseType(InEaseType, InTime, InDuration);
}

float UMathStatics::BounceEaseIn(float InTime, float InDuration)
{
	return FMathHelper::BounceEaseIn(InTime, InDuration);
}

float UMathStatics::BounceEaseInOut(float InTime, float InDuration)
{
	return FMathHelper::BounceEaseInOut(InTime, InDuration);
}

float UMathStatics::BounceEaseOut(float InTime, float InDuration)
{
	return FMathHelper::BounceEaseOut(InTime, InDuration);
}

float UMathStatics::GetNoise1D(float InValue, int32 InOffset, bool bAbs, bool bUnsigned)
{
	return FMathHelper::GetNoise1D(InValue, InOffset, bAbs, bUnsigned);
}

float UMathStatics::GetNoise2D(FVector2D InLocation, int32 InOffset, bool bAbs, bool bUnsigned)
{
	return FMathHelper::GetNoise2D(InLocation, InOffset, bAbs, bUnsigned);
}

float UMathStatics::GetNoise3D(FVector InLocation, int32 InOffset, bool bAbs, bool bUnsigned)
{
	return FMathHelper::GetNoise3D(InLocation, InOffset, bAbs, bUnsigned);
}
