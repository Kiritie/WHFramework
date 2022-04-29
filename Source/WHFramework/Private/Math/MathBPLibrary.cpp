// Fill out your copyright notice in the Description page of Project Settings.


#include "Math/MathBPLibrary.h"

EDirection UMathBPLibrary::InvertDirection(EDirection InDirection)
{
	if ((int32)InDirection % 2 == 0)
		return (EDirection)((int32)InDirection + 1);
	else
		return (EDirection)((int32)InDirection - 1);
}

FVector UMathBPLibrary::DirectionToVector(EDirection InDirection, FRotator InRotation /*= FRotator::ZeroRotator*/)
{
	switch (InDirection)
	{
		case EDirection::Up:
			return InRotation.RotateVector(FVector::UpVector);
		case EDirection::Down:
			return InRotation.RotateVector(FVector::DownVector);
		case EDirection::Forward:
			return InRotation.RotateVector(FVector::ForwardVector);
		case EDirection::Back:
			return InRotation.RotateVector(FVector::BackwardVector);
		case EDirection::Left:
			return InRotation.RotateVector(FVector::LeftVector);
		case EDirection::Right:
			return InRotation.RotateVector(FVector::RightVector);
	}
	return FVector::ZeroVector;
}

FIndex UMathBPLibrary::DirectionToIndex(EDirection InDirection, FRotator InRotation /*= FRotator::ZeroRotator*/)
{
	return FIndex(DirectionToVector(InDirection, InRotation));
}

FIndex UMathBPLibrary::GetAdjacentIndex(FIndex InIndex, EDirection InDirection, FRotator InRotation /*= FRotator::ZeroRotator*/)
{
	return InIndex + DirectionToIndex(InDirection, InRotation);
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
		return BounceEaseIn(InTime, InDuration, overshootOrAmplitude, period);

	case EEaseType::OutBounce:
		return BounceEaseOut(InTime, InDuration, overshootOrAmplitude, period);

	case EEaseType::InOutBounce:
		return BounceEaseInOut(InTime, InDuration, overshootOrAmplitude, period);
	}
	InTime = InTime / InDuration;
	return (-InTime * (InTime - 2.0));
}

float UMathBPLibrary::BounceEaseIn(float InTime, float InDuration, float InUnusedOvershootOrAmplitude, float InUnusedPeriod)
{
	return 1.0 - BounceEaseOut(InDuration - InTime, InDuration, -1.0, -1.0);
}

float UMathBPLibrary::BounceEaseInOut(float InTime, float InDuration, float InUnusedOvershootOrAmplitude, float InUnusedPeriod)
{
	return  (InTime >= (InDuration * 0.5f)) ? ((BounceEaseOut((InTime * 2.0) - InDuration, InDuration, -1.0, -1.0) * 0.5f) + 0.5f) : (BounceEaseIn(InTime * 2.0, InDuration, -1.0, -1.0) * 0.5f);
}

float UMathBPLibrary::BounceEaseOut(float InTime, float InDuration, float InUnusedOvershootOrAmplitude, float InUnusedPeriod)
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
