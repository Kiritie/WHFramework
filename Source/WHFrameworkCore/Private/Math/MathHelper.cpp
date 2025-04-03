// Fill out your copyright notice in the Description page of Project Settings.

#include "Math/MathHelper.h"

uint64 FMathHelper::Index(int32 InX, int32 InY, int32 InZ)
{
	const int32 Offset = 16384;
	uint64 T =
		uint64(InX + Offset) << 40 |
		uint64(InY + Offset) << 20 |
		uint64(InZ + Offset);
	return T;
}

uint64 FMathHelper::Index(FIndex InIndex)
{
	return Index(InIndex.X, InIndex.Y, InIndex.Z);
}

FIndex FMathHelper::UnIndex(uint64 InIndex)
{
	const int32 Offset = 16384;
	return FIndex(
		int32(InIndex >> 40) - Offset,
		int32((InIndex >> 20) & 0xFFFFF) - Offset,
		int32(InIndex & 0xFFFFF) - Offset
	);
}

FIndex FMathHelper::RotateIndex(const FIndex& InIndex, ERightAngle InAngle, bool bAbsolute)
{
	return RotateVector(InIndex, InAngle, true, bAbsolute);
}

FVector FMathHelper::RotateVector(const FVector& InVector, const FRotator& InRotator, bool bRound, bool bAbsolute)
{
	FVector Vector = !InRotator.IsZero() ? InRotator.RotateVector(InVector) : InVector;
	if(bRound)
	{
		Vector.X = FMath::RoundToFloat(Vector.X);
		Vector.Y = FMath::RoundToFloat(Vector.Y);
		Vector.Z = FMath::RoundToFloat(Vector.Z);
	}
	if(bAbsolute)
	{
		Vector.X = FMath::Abs(Vector.X);
		Vector.Y = FMath::Abs(Vector.Y);
		Vector.Z = FMath::Abs(Vector.Z);
	}
	return Vector;
}

FVector FMathHelper::RotateVector(const FVector& InVector, ERightAngle InAngle, bool bRound, bool bAbsolute)
{
	return RotateVector(InVector, FRotator(0.f, (int32)InAngle * 90.f, 0.f), bRound, bAbsolute);
}

bool FMathHelper::IsPointInEllipse2D(const FVector2D& InPoint, const FVector2D& InCenter, const FVector2D& InRadius)
{
	FVector2D Delta = FVector2D(InPoint.X - InCenter.X, InPoint.Y - InCenter.Y);
	Delta.Y /= InRadius.Y / InRadius.X;
	return Delta.Length() < InRadius.X;
}

bool FMathHelper::IsPointInPolygon2D(const FVector2D& InPoint, const TArray<FVector2D>& InPoints)
{
	if (InPoints.Num() < 3)
	{
		return false; // 不是有效的多边形
	}

	bool bResult = false;
	const int32 NumPoints = InPoints.Num();
    
	for (int32 i = 0, j = NumPoints - 1; i < NumPoints; j = i++)
	{
		const FVector2D& PointI = InPoints[i];
		const FVector2D& PointJ = InPoints[j];
        
		// 检查点是否在边的Y范围内
		const bool bWithinYRange = (PointI.Y > InPoint.Y) != (PointJ.Y > InPoint.Y);
		if (bWithinYRange)
		{
			// 计算X轴交点
			const float XIntersect = (PointJ.X - PointI.X) * (InPoint.Y - PointI.Y) / (PointJ.Y - PointI.Y) + PointI.X;
            
			// 如果测试点在交点左侧，切换结果
			if (InPoint.X < XIntersect)
			{
				bResult = !bResult;
			}
		}
	}
    
	return bResult;
}

float FMathHelper::RightAngleToFloat(ERightAngle InAngle)
{
	return (int32)InAngle * 90.f;
}

ERightAngle FMathHelper::FloatToRightAngle(float InAngle)
{
	InAngle = FMath::Fmod(InAngle, 360.f);
	if (InAngle < 0.f)
	{
		InAngle += 360.f;
	}
	return (ERightAngle)(FMath::RoundToInt(InAngle / 90.f) % 4);
}

ERightAngle FMathHelper::GetOffsetRightAngle(ERightAngle InAngle, int32 InOffset)
{
	const int32 Angle = FMath::Abs((int32)InAngle + InOffset) % 4;
	return (ERightAngle)Angle;
}

ERightAngle FMathHelper::CombineRightAngle(ERightAngle InAngle1, ERightAngle InAngle2)
{
	return (ERightAngle)(((int32)InAngle1 + (int32)InAngle2) % 4);
}

EDirection FMathHelper::InvertDirection(EDirection InDirection)
{
	switch (InDirection)
	{
		case EDirection::Forward:
			return EDirection::Backward;
		case EDirection::Right:
			return EDirection::Left;
		case EDirection::Backward:
			return EDirection::Forward;
		case EDirection::Left:
			return EDirection::Right;
		case EDirection::Up:
			return EDirection::Down;
		case EDirection::Down:
			return EDirection::Up;
		default: break;
	}
	return InDirection;
}

EDirection FMathHelper::RotateDirection(EDirection InDirection, ERightAngle InAngle)
{
	switch (InDirection)
	{
		case EDirection::Forward:
		case EDirection::Right:
		case EDirection::Backward:
		case EDirection::Left:
		{
			const int32 Direction = (int32)InDirection + (int32)InAngle;
			return EDirection(Direction < 4 ? Direction : Direction - 4);
		}
		case EDirection::Up:
			return EDirection::Up;
		case EDirection::Down:
			return EDirection::Down;
		default: break;
	}
	return InDirection;
}

FVector FMathHelper::DirectionToVector(EDirection InDirection, ERightAngle InAngle)
{
	switch (RotateDirection(InDirection, InAngle))
	{
		case EDirection::Forward:
			return FVector::ForwardVector;
		case EDirection::Right:
			return FVector::RightVector;
		case EDirection::Backward:
			return FVector::BackwardVector;
		case EDirection::Left:
			return FVector::LeftVector;
		case EDirection::Up:
			return FVector::UpVector;
		case EDirection::Down:
			return FVector::DownVector;
		default: break;
	}
	return FVector::ZeroVector;
}

FIndex FMathHelper::DirectionToIndex(EDirection InDirection, ERightAngle InAngle)
{
	return FIndex(DirectionToVector(InDirection, InAngle));
}

FIndex FMathHelper::GetAdjacentIndex(FIndex InIndex, EDirection InDirection, ERightAngle InAngle)
{
	return InIndex + DirectionToIndex(InDirection, InAngle);
}

float FMathHelper::EvaluateByCurve(UCurveFloat* InCurve, float InTime, float InDuration)
{
	if (InCurve != nullptr)
	{
		float MinTime;
		float MaxTime;
		InCurve->GetTimeRange(MinTime,MaxTime);
		float CurveDurTime = MaxTime - MinTime;
		if (CurveDurTime == 0)
		{
			return 0;
		}
		float BiliCurTime = MinTime + (InTime / InDuration * CurveDurTime);
		return InCurve->GetFloatValue(BiliCurTime);
	}
	return 0.f;
}
	
float FMathHelper::EvaluateByEaseType(EEaseType InEaseType, float InTime, float InDuration)
{
	float period = 0.f;
	float overshootOrAmplitude = 1.70158f;
	switch (InEaseType)
	{
		case EEaseType::Linear:
			return (InTime / InDuration);
		case EEaseType::InSine:
			return (-((float)FMath::Cos((double)((InTime / InDuration) * 1.570796f))) + 1.0);
		case EEaseType::OutSine:
			return (float)FMath::Sin((double)((InTime / InDuration) * 1.570796f));
		case EEaseType::InOutSine:
			return (-0.5f * (((float)FMath::Cos((double)((3.141593f * InTime) / InDuration))) - 1.0));
		case EEaseType::InQuad:
		{
			InTime = InTime / InDuration;
			return (InTime * InTime);
		}
		case EEaseType::OutQuad:
		{
			InTime = InTime / InDuration;
			return (-InTime * (InTime - 2.0));
		}
		case EEaseType::InOutQuad:
		{
			InTime = InTime / (InDuration * 0.5f);
			if (InTime < 1.0)
			{
				return ((0.5f * InTime) * InTime);
			}
			InTime = InTime - 1.0;
			return (-0.5f * ((InTime * (InTime - 2.0)) - 1.0));
		}
		case EEaseType::InCubic:
		{
			InTime = InTime / InDuration;
			return ((InTime * InTime) * InTime);
		}
		case EEaseType::OutCubic:
		{
			InTime = (InTime / InDuration) - 1.0;
			return (((InTime * InTime) * InTime) + 1.0);
		}
		case EEaseType::InOutCubic:
		{
			InTime = InTime / (InDuration * 0.5f);
			if (InTime < 1.0)
			{
				return (((0.5f * InTime) * InTime) * InTime);
			}
			InTime = InTime - 2.0;
			return (0.5f * (((InTime * InTime) * InTime) + 2.0));
		}
		case EEaseType::InQuart:
		{
			InTime = InTime / InDuration;
			return (((InTime * InTime) * InTime) * InTime);
		}
		case EEaseType::OutQuart:
		{
			InTime = (InTime / InDuration) - 1.0;
			return -((((InTime * InTime) * InTime) * InTime) - 1.0);
		}
		case EEaseType::InOutQuart:
		{
			InTime = InTime / (InDuration * 0.5f);
			if (InTime < 1.0)
			{
				return ((((0.5f * InTime) * InTime) * InTime) * InTime);
			}
			InTime = InTime - 2.0;
			return (-0.5f * ((((InTime * InTime) * InTime) * InTime) - 2.0));
		}
		case EEaseType::InQuint:
		{
			InTime = InTime / InDuration;
			return ((((InTime * InTime) * InTime) * InTime) * InTime);
		}
		case EEaseType::OutQuint:
		{
			InTime = (InTime / InDuration) - 1.0;
			return (((((InTime * InTime) * InTime) * InTime) * InTime) + 1.0);
		}
		case EEaseType::InOutQuint:
		{
			InTime = InTime / (InDuration * 0.5f);
			if (InTime < 1.0)
			{
				return (((((0.5f * InTime) * InTime) * InTime) * InTime) * InTime);
			}
			InTime = InTime - 2.0;
			return (0.5f * (((((InTime * InTime) * InTime) * InTime) * InTime) + 2.0));
		}
		case EEaseType::InExpo:
			return ((InTime == 0.0) ? 0.0 : ((float)FMath::Pow(2.0, (double)(10.0 * ((InTime / InDuration) - 1.0)))));
		case EEaseType::OutExpo:
			return ((InTime != InDuration) ? (-((float)FMath::Pow(2.0, (double)((-10.0 * InTime) / InDuration))) + 1.0) : 1.0);
		case EEaseType::InOutExpo:
		{
			if (InTime == 0.0)
			{
				return 0.0;
			}
			if (InTime == InDuration)
			{
				return 1.0;
			}
			InTime = InTime / (InDuration * 0.5f);
			if (InTime < 1.0)
			{
				return (0.5f * ((float)FMath::Pow(2.0, (double)(10.0 * (InTime - 1.0)))));
			}
			InTime = InTime - 1.0;
			return (0.5f * (-((float)FMath::Pow(2.0, (double)(-10.0 * InTime))) + 2.0));
		}
		case EEaseType::InCirc:
		{
			InTime = InTime / InDuration;
			return -(((float)FMath::Sqrt((double)(1.0 - (InTime * InTime)))) - 1.0);
		}
		case EEaseType::OutCirc:
		{
			InTime = (InTime / InDuration) - 1.0;
			return (float)FMath::Sqrt((double)(1.0 - (InTime * InTime)));
		}
		case EEaseType::InOutCirc:
		{
			InTime = InTime / (InDuration * 0.5f);
			if (InTime < 1.0)
			{
				return (-0.5f * (((float)FMath::Sqrt((double)(1.0 - (InTime * InTime)))) - 1.0));
			}
			InTime = InTime - 2.0;
			return (0.5f * (((float)FMath::Sqrt((double)(1.0 - (InTime * InTime)))) + 1.0));
		}
		case EEaseType::InElastic:
		{
			float num;
			if (InTime == 0.0)
			{
				return 0.0;
			}
			InTime = InTime / InDuration;
			if (InTime == 1.0)
			{
				return 1.0;
			}
			if (period == 0.0)
			{
				period = InDuration * 0.3f;
			}
			if (overshootOrAmplitude >= 1.0)
			{
				num = (period / 6.283185f) * ((float)FMath::Asin((double)(1.0 / overshootOrAmplitude)));
			}
			else
			{
				overshootOrAmplitude = 1.0;
				num = period / 4.0;
			}
			InTime = InTime - 1.0;
			return -((overshootOrAmplitude * ((float)FMath::Pow(2.0, (double)(10.0 * InTime )))) * ((float)FMath::Sin((double)((((InTime * InDuration) - num) * 6.283185f) / period))));
		}
		case EEaseType::OutElastic:
		{
			float num2;
			if (InTime == 0.0)
			{
				return 0.0;
			}
			InTime = InTime / InDuration;
			if (InTime == 1.0)
			{
				return 1.0;
			}
			if (period == 0.0)
			{
				period = InDuration * 0.3f;
			}
			if (overshootOrAmplitude >= 1.0)
			{
				num2 = (period / 6.283185f) * ((float)FMath::Asin((double)(1.0 / overshootOrAmplitude)));
			}
			else
			{
				overshootOrAmplitude = 1.0;
				num2 = period / 4.0;
			}
			return (((overshootOrAmplitude * ((float)FMath::Pow(2.0, (double)(-10.0 * InTime)))) * ((float)FMath::Sin((double)((((InTime * InDuration) - num2) * 6.283185f) / period)))) + 1.0);
		}
		case EEaseType::InOutElastic:
		{
			float num3;
			if (InTime == 0.0)
			{
				return 0.0;
			}
			InTime = InTime / (InDuration * 0.5f);
			if (InTime == 2.0)
			{
				return 1.0;
			}
			if (period == 0.0)
			{
				period = InDuration * 0.45f;
			}
			if (overshootOrAmplitude >= 1.0)
			{
				num3 = (period / 6.283185f) * ((float)FMath::Asin((double)(1.0 / overshootOrAmplitude)));
			}
			else
			{
				overshootOrAmplitude = 1.0;
				num3 = period / 4.0;
			}
			if (InTime < 1.0)
			{
				InTime = InTime - 1.0;
				return (-0.5f * ((overshootOrAmplitude * ((float)FMath::Pow(2.0, (double)(10.0 * InTime)))) * ((float)FMath::Sin((double)((((InTime * InDuration) - num3) * 6.283185f) / period)))));
			}
			InTime = InTime - 1.0;
			return ((((overshootOrAmplitude * ((float)FMath::Pow(2.0, (double)(-10.0 * InTime)))) * ((float)FMath::Sin((double)((((InTime * InDuration) - num3) * 6.283185f) / period)))) * 0.5f) + 1.0);
		}
		case EEaseType::InBack:
		{
			InTime = InTime / InDuration;
			return ((InTime * InTime) * (((overshootOrAmplitude + 1.0) * InTime) - overshootOrAmplitude));
		}
		case EEaseType::OutBack:
		{
			InTime = (InTime / InDuration) - 1.0;
			return (((InTime * InTime) * (((overshootOrAmplitude + 1.0) * InTime) + overshootOrAmplitude)) + 1.0);
		}
		case EEaseType::InOutBack:
		{
			InTime = InTime / (InDuration * 0.5f);
			if (InTime < 1.0)
			{
				overshootOrAmplitude = overshootOrAmplitude * 1.525f;
				return (0.5f * ((InTime * InTime) * (((overshootOrAmplitude + 1.0) * InTime) - overshootOrAmplitude)));
			}
			InTime = InTime - 2.0;
			overshootOrAmplitude = overshootOrAmplitude * 1.525f;
			return (0.5f * (((InTime * InTime) * (((overshootOrAmplitude + 1.0) * InTime) + overshootOrAmplitude)) + 2.0));
		}
		case EEaseType::InBounce:
			return BounceEaseIn(InTime, InDuration);
		case EEaseType::OutBounce:
			return BounceEaseOut(InTime, InDuration);
		case EEaseType::InOutBounce:
			return BounceEaseInOut(InTime, InDuration);
	}
	InTime = InTime / InDuration;
	return (-InTime * (InTime - 2.0));
}

float FMathHelper::BounceEaseIn(float InTime, float InDuration)
{
	return 1.0 - BounceEaseOut(InDuration - InTime, InDuration);
}

float FMathHelper::BounceEaseInOut(float InTime, float InDuration)
{
	return  (InTime >= (InDuration * 0.5f)) ? ((BounceEaseOut((InTime * 2.0) - InDuration, InDuration) * 0.5f) + 0.5f) : (BounceEaseIn(InTime * 2.0, InDuration) * 0.5f);
}

float FMathHelper::BounceEaseOut(float InTime, float InDuration)
{
	InTime = InTime / InDuration;
	if (InTime < 0.3636364f)
	{
		return ((7.5625f * InTime) * InTime);
	}
	if (InTime < 0.7272727f)
	{
		InTime = InTime - 0.5454546f;
		return (((7.5625f * InTime) * InTime) + 0.75f);
	}
	if (InTime < 0.9090909f)
	{
		InTime = InTime - 0.8181818f;
		return (((7.5625f * InTime) * InTime) + 0.9375f);
	}
	InTime = InTime - 0.9545454f;
	return (((7.5625f * InTime) * InTime) + 0.984375f);
}

float FMathHelper::GetNoise1D(float InValue, int32 InOffset, bool bAbs, bool bUnsigned)
{
	const float Noise1D = FMath::PerlinNoise1D(InValue + InOffset) + (bUnsigned ? 1.f : 0.f);
	return bUnsigned ? (Noise1D * 0.5f) : (bAbs ? FMath::Abs(Noise1D) : Noise1D);
}

float FMathHelper::GetNoise2D(FVector2D InLocation, int32 InOffset, bool bAbs, bool bUnsigned)
{
	const float Noise2D = FMath::PerlinNoise2D(FVector2D(InLocation.X + InOffset, InLocation.Y + InOffset)) + (bUnsigned ? 1.f : 0.f);
	return bUnsigned ? (Noise2D * 0.5f) : (bAbs ? FMath::Abs(Noise2D) : Noise2D);
}

float FMathHelper::GetNoise3D(FVector InLocation, int32 InOffset, bool bAbs, bool bUnsigned)
{
	const float Noise3D = FMath::PerlinNoise3D(FVector(InLocation.X + InOffset, InLocation.Y + InOffset, InLocation.Z) + (bUnsigned ? 1.f : 0.f));
	return bUnsigned ? (Noise3D * 0.5f) : (bAbs ? FMath::Abs(Noise3D) : Noise3D);
}

int32 FMathHelper::Hash11(int32 InValue)
{
	const uint32 BIT_NOISE1 = 0x85297A4D;
	const uint32 BIT_NOISE2 = 0x68E31DA4;
	const uint32 BIT_NOISE3 = 0x1B56C4E9;
	uint32 Mangled = InValue;
	Mangled *= BIT_NOISE1;
	Mangled ^= (Mangled >> 8);
	Mangled += BIT_NOISE2;
	Mangled ^= (Mangled << 8);
	Mangled *= BIT_NOISE3;
	Mangled ^= (Mangled >> 8);
	return Mangled % 1024;
}

int32 FMathHelper::Hash11WithSeed(int32 InValue, int32 InSeed)
{
	const uint32 BIT_NOISE1 = 0x85297A4D;
	const uint32 BIT_NOISE2 = 0x68E31DA4;
	const uint32 BIT_NOISE3 = 0x1B56C4E9;
	uint32 Mangled = InValue ^ InSeed;
	Mangled *= BIT_NOISE1;
	Mangled ^= (Mangled >> 8);
	Mangled += BIT_NOISE2;
	Mangled ^= (Mangled << 8);
	Mangled *= BIT_NOISE3;
	Mangled ^= (Mangled >> 8);
	return Mangled % 1024;
}

FVector2D FMathHelper::Hash22(FVector2D InLocation)
{
	FVector2D V(
		Hash11(0x651A6BE3 * (int32)InLocation.X - (int32)InLocation.Y) % 1024,
		Hash11((int32)InLocation.X * (int32)InLocation.Y + 0x218AE247) % 1024
	);
	V /= 1024.0f;
	return V;
}

int32 FMathHelper::Hash21(FVector2D InLocation)
{
	return Hash11(0x651A6BE1 * (int32)InLocation.X + (int32)InLocation.Y) % 1024;
}

FVector FMathHelper::Hash33(FVector InLocation)
{
	FVector V(
		Hash11((int32)InLocation.X ^ 0x651A6BE3 + (int32)InLocation.Y ^ 0x218A6147 - (int32)InLocation.Z ^ 0x118A5191) % 1024,
		Hash11((int32)InLocation.X ^ 0x118A5191 - (int32)InLocation.Y ^ 0x218AE247 + (int32)InLocation.Z ^ 0x2B8AE147) % 1024,
		Hash11((int32)InLocation.X ^ 0x21613122 - (int32)InLocation.Y ^ 0x118A5191 - (int32)InLocation.Z ^ 0x218AE247) % 1024
	);
	V /= 1024.0f;
	return V;
}

int32 FMathHelper::Hash31(FVector InLocation)
{
	return Hash11(InLocation.X * 0x651A6BE6 - InLocation.Y * 0xCB251062 + InLocation.Z);
}

int32 FMathHelper::HashRandInt(FVector2D InLocation, int32 InSeed)
{
	InLocation = InLocation * 1024;
	return Hash11WithSeed(0x651A6BE1 * (int32)InLocation.X + (int32)InLocation.Y, InSeed) % 1024;
}

float FMathHelper::HashRand(FVector2D InLocation, int32 InSeed)
{
	InLocation = InLocation * 1024;
	return Hash11WithSeed(0x651A6BE1 * (int32)InLocation.X + (int32)InLocation.Y, InSeed) % 1024 / 1024.0f;
}

int32 FMathHelper::HashRandRange(FVector2D InLocation, int32 InMinValue, int32 InMaxValue, int32 InSeed)
{
	const int32 Range = (InMaxValue - InMinValue) + 1;
	return InMinValue + HashRandHelper(InLocation, Range, InSeed);
}

float FMathHelper::HashRandRange(FVector2D InLocation, float InMinValue, float InMaxValue, int32 InSeed)
{
	return InMinValue + (InMaxValue - InMinValue) * HashRand(InLocation, InSeed);
}

FVector2D FMathHelper::Bezier(FVector2D InP0, FVector2D InP1, FVector2D InP2, float InT)
{
	return ((1 - InT) * (1 - InT) * InP0 + InT * (1 - InT) * InP1 + InT * InT * InP2);
}

FVector2D FMathHelper::Bezier(FVector2D InP0, FVector2D InP1, FVector2D InP2, FVector2D InP3, float InT)
{
	return Bezier(InP0, InP1, InP2, InT) * (1 - InT) + Bezier(InP1, InP2, InP3, InT) * InT;
}

int32 FMathHelper::HashRandHelper(FVector2D InLocation, int32 InValue, int32 InSeed)
{
	return InValue > 0 ? FMath::Min(FMath::TruncToInt(HashRand(InLocation, InSeed) * (float)InValue), InValue - 1) : 0;
}
