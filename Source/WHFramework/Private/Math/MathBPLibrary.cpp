// Fill out your copyright notice in the Description page of Project Settings.


#include "Math/MathBPLibrary.h"

FVector UMathBPLibrary::RotatorVector(const FVector& Vector, const FRotator& Rotator, bool bRound, bool bAbsolute)
{
	FVector vector = !Rotator.IsZero() ? Rotator.RotateVector(Vector) : Vector;
	if(bRound)
	{
		vector.X = FMath::RoundToFloat(vector.X);
		vector.Y = FMath::RoundToFloat(vector.Y);
		vector.Z = FMath::RoundToFloat(vector.Z);
	}
	if(bAbsolute)
	{
		vector.X = FMath::Abs(vector.X);
		vector.Y = FMath::Abs(vector.Y);
		vector.Z = FMath::Abs(vector.Z);
	}
	return vector;
}

FVector UMathBPLibrary::RotatorVector(const FVector& Vector, ERightAngle Angle, bool bRound, bool bAbsolute)
{
	return RotatorVector(Vector, FRotator(0.f, (int32)Angle * 90.f, 0.f), bRound, bAbsolute);
}

float UMathBPLibrary::RightAngleToFloat(ERightAngle InAngle)
{
	return (int32)InAngle * 90.f;
}

ERightAngle UMathBPLibrary::GetOffsetRightAngle(ERightAngle InAngle, int32 InOffset)
{
	const int32 Angle = FMath::Abs((int32)InAngle + InOffset) % 4;
	return (ERightAngle)Angle;
}

EDirection UMathBPLibrary::InvertDirection(EDirection InDirection)
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
	}
	return InDirection;
}

EDirection UMathBPLibrary::RotateDirection(EDirection InDirection, ERightAngle InAngle)
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
	}
	return InDirection;
}

FVector UMathBPLibrary::DirectionToVector(EDirection InDirection, ERightAngle InAngle)
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
	}
	return FVector::ZeroVector;
}

FIndex UMathBPLibrary::DirectionToIndex(EDirection InDirection, ERightAngle InAngle)
{
	return FIndex(DirectionToVector(InDirection, InAngle));
}

FIndex UMathBPLibrary::GetAdjacentIndex(FIndex InIndex, EDirection InDirection, ERightAngle InAngle)
{
	return InIndex + DirectionToIndex(InDirection, InAngle);
}

float UMathBPLibrary::EvaluateByCurve(UCurveFloat* InCurve, float InTime, float InDuration)
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
	
float UMathBPLibrary::EvaluateByEaseType(EEaseType InEaseType, float InTime, float InDuration)
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

float UMathBPLibrary::BounceEaseIn(float InTime, float InDuration)
{
	return 1.0 - BounceEaseOut(InDuration - InTime, InDuration);
}

float UMathBPLibrary::BounceEaseInOut(float InTime, float InDuration)
{
	return  (InTime >= (InDuration * 0.5f)) ? ((BounceEaseOut((InTime * 2.0) - InDuration, InDuration) * 0.5f) + 0.5f) : (BounceEaseIn(InTime * 2.0, InDuration) * 0.5f);
}

float UMathBPLibrary::BounceEaseOut(float InTime, float InDuration)
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

float UMathBPLibrary::GetNoiseHeight(FVector2D InLocation, FVector InScale, int32 InOffset, bool bUnsigned)
{
	return (FMath::PerlinNoise2D(FVector2D((InLocation.X + InOffset) * InScale.X, (InLocation.Y + InOffset) * InScale.Y)) + (bUnsigned ? 1.f : 0.f)) * InScale.Z;
}
