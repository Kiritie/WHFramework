// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "MathTypes.generated.h"

#define DEG_2_RAD (0.01745329f)

#define Vector_Empty FVector(MAX_flt)

UENUM(BlueprintType)
enum class EEaseType : uint8
{
	Linear = 0,
	InSine = 1,
	InQuad = 2,
	InCubic = 3,
	InQuart = 4,
	InQuint = 5,
	InExpo = 6,
	InCirc = 7,
	InElastic = 8,
	InBack = 9,
	InBounce = 10,
	OutQuad = 31,
	OutSine = 32,
	OutCubic = 33,
	OutQuart = 34,
	OutQuint = 35,
	OutExpo = 36,
	OutCirc = 37,
	OutElastic = 38,
	OutBack = 39,
	OutBounce = 40,
	InOutQuad = 61,
	InOutSine = 62,
	InOutCubic = 63,
	InOutQuart = 64,
	InOutQuint = 65,
	InOutExpo = 66,
	InOutCirc = 67,
	InOutElastic = 68,
	InOutBack = 69,
	InOutBounce = 70
};

/**
 * ????
 */
UENUM(BlueprintType)
enum class EDirection : uint8
{
	Up,
	Down,
	Forward,
	Back,
	Left,
	Right
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FIndex
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 X;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Z;

	static const FIndex ZeroIndex;

	static const FIndex OneIndex;

	FORCEINLINE FIndex()
	{
		X = 0;
		Y = 0;
		Z = 0;
	}

	FORCEINLINE FIndex(int32 InX, int32 InY, int32 InZ)
	{
		X = InX;
		Y = InY;
		Z = InZ;
	}

	FORCEINLINE FIndex(FVector InVector)
	{
		X = FMath::CeilToInt(InVector.X);
		Y = FMath::CeilToInt(InVector.Y);
		Z = FMath::CeilToInt(InVector.Z);
	}

	FORCEINLINE FIndex(const FString& InValue)
	{
		TArray<FString> tmpArr;
		InValue.ParseIntoArray(tmpArr, TEXT(","));
		X = FCString::Atoi(*tmpArr[0]);
		Y = FCString::Atoi(*tmpArr[1]);
		Z = FCString::Atoi(*tmpArr[2]);
	}

	FORCEINLINE FVector ToVector() const
	{
		return FVector(X, Y, Z);
	}

	FORCEINLINE FString ToString() const
	{
		return FString::Printf(TEXT("%d,%d,%d"), X, Y, Z);
	}

	FORCEINLINE static float Distance(FIndex A, FIndex B, bool bIgnoreZ = false)
	{
		if (bIgnoreZ) A.Z = B.Z = 0;
		return FVector::Distance(A.ToVector(), B.ToVector());
	}

	FORCEINLINE friend bool operator==(const FIndex& A, const FIndex& B)
	{
		return (A.X == B.X) && (A.Y == B.Y) && (A.Z == B.Z);
	}

	FORCEINLINE friend bool operator!=(const FIndex& A, const FIndex& B)
	{
		return (A.X != B.X) || (A.Y != B.Y) || (A.Z != B.Z);
	}

	FORCEINLINE FIndex operator+(const FIndex& InIndex) const
	{
		return FIndex(X + InIndex.X, Y + InIndex.Y, Z + InIndex.Z);
	}

	FORCEINLINE FIndex operator-(const FIndex& InIndex) const
	{
		return FIndex(X - InIndex.X, Y - InIndex.Y, Z - InIndex.Z);
	}

	FORCEINLINE FIndex operator*(const FIndex& InIndex) const
	{
		return FIndex(X * InIndex.X, Y * InIndex.Y, Z * InIndex.Z);
	}

	FORCEINLINE FIndex operator/(const FIndex& InIndex) const
	{
		return FIndex(X / InIndex.X, Y / InIndex.Y, Z / InIndex.Z);
	}

	FORCEINLINE FIndex operator*(const int& InValue) const
	{
		return FIndex(X * InValue, Y * InValue, Z * InValue);
	}

	FORCEINLINE FIndex operator/(const int& InValue) const
	{
		return FIndex(X / InValue, Y / InValue, Z / InValue);
	}

	friend void operator<<(FStructuredArchive::FSlot Slot, FIndex& InValue)
	{
		FStructuredArchive::FRecord Record = Slot.EnterRecord();
		Record << SA_VALUE(TEXT("X"), InValue.X);
		Record << SA_VALUE(TEXT("Y"), InValue.Y);
		Record << SA_VALUE(TEXT("Z"), InValue.Z);
	}
};

FORCEINLINE uint32 GetTypeHash(const FIndex& InIndex)
{
	return FCrc::MemCrc_DEPRECATED(&InIndex, sizeof(InIndex));
}

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FPoint
{
	GENERATED_BODY()

public:
	FORCEINLINE FPoint()
	{
		X = 0;
		Y = 0;
	}

	FORCEINLINE FPoint(double x, double y)
	{
		X = x;
		Y = y;
	}

	FORCEINLINE FPoint(FVector v)
	{
		X = v.Y;
		Y = v.X;
	}

public:
	double X;
	
	double Y;

	static const FPoint Zero;

	static const FPoint One;

	static const FPoint Up;

	static const FPoint Right;

	FORCEINLINE void Set(double new_x, double new_y)
	{
		X = new_x;
		Y = new_y;
	}

	FORCEINLINE void Scale(FPoint scale)
	{
		X *= scale.X;
		Y *= scale.Y;
	}

	FORCEINLINE FPoint Normalize() const
	{
		double magnitude = Magnitude();
		if (magnitude > 9.99999974737875E-06)
		{
			return FPoint(X, Y) / magnitude;
		}
		else
		{
			return FPoint::Zero;
		}
	}

	FORCEINLINE FVector ToVector() const
	{
		return FVector(Y, X, 0.f);
	}

	FORCEINLINE FString ToString() const
	{
		return FString::Printf(TEXT("%f,%f"), X, Y);
	}

	FORCEINLINE double Magnitude() const
	{
		return FMath::Sqrt(X * X + Y * Y);
	}

	FORCEINLINE double SqrMagnitude() const
	{
		return (X * X + Y * Y);
	}

	FORCEINLINE friend FPoint operator +(FPoint a, FPoint b)
	{
		return FPoint(a.X + b.X, a.Y + b.Y);
	}

	FORCEINLINE friend FPoint operator -(FPoint a, FPoint b)
	{
		return FPoint(a.X - b.X, a.Y - b.Y);
	}

	FORCEINLINE friend FPoint operator -(FPoint a)
	{
		return FPoint(-a.X, -a.Y);
	}

	FORCEINLINE friend FPoint operator *(FPoint a, double d)
	{
		return FPoint(a.X * d, a.Y * d);
	}

	FORCEINLINE friend FPoint operator *(double d, FPoint a)
	{
		return FPoint(a.X * d, a.Y * d);
	}

	FORCEINLINE friend FPoint operator /(FPoint a, double d)
	{
		return FPoint(a.X / d, a.Y / d);
	}

	FORCEINLINE friend bool operator ==(FPoint lhs, FPoint rhs)
	{
		return FPoint::SqrMagnitude(lhs - rhs) < 0.f / 1.f;
	}

	FORCEINLINE friend bool operator !=(FPoint lhs, FPoint rhs)
	{
		return (double)FPoint::SqrMagnitude(lhs - rhs) >= 0.f / 1.f;
	}
	
	friend void operator<<(FStructuredArchive::FSlot Slot, FPoint& InPoint)
	{
		FStructuredArchive::FRecord Record = Slot.EnterRecord();
		Record << SA_VALUE(TEXT("X"), InPoint.X);
		Record << SA_VALUE(TEXT("Y"), InPoint.Y);
	}

public:
	FORCEINLINE static FPoint Lerp(FPoint from, FPoint to, double t)
	{
		t = FMath::Clamp(t, 0.0, 1.0);
		return FPoint(from.X + (to.X - from.X) * t, from.Y + (to.Y - from.Y) * t);
	}

	FORCEINLINE static FPoint MoveTowards(FPoint current, FPoint target, double maxDistanceDelta)
	{
		FPoint vector2 = target - current;
		double magnitude = vector2.Magnitude();
		if (magnitude <= maxDistanceDelta || magnitude == 0.f)
			return target;
		else
			return current + vector2 / magnitude * maxDistanceDelta;
	}

	FORCEINLINE static FPoint Scale(FPoint a, FPoint b)
	{
		return FPoint(a.X * b.X, a.Y * b.Y);
	}

	FORCEINLINE static double Dot(FPoint lhs, FPoint rhs)
	{
		return lhs.X * rhs.X + lhs.Y * rhs.Y;
	}

	FORCEINLINE static double Angle(FPoint from, FPoint to)
	{
		return FMath::Acos(FMath::Clamp(FPoint::Dot(from.Normalize(), to.Normalize()), -1.0, 1.0)) * 57.29578;
	}

	FORCEINLINE static double Distance(FPoint a, FPoint b)
	{
		return (a - b).Magnitude();
	}

	FORCEINLINE static FPoint ClampMagnitude(FPoint vector, double maxLength)
	{
		if (vector.SqrMagnitude() > maxLength * maxLength)
			return vector.Normalize() * maxLength;
		else
			return vector;
	}

	FORCEINLINE static double SqrMagnitude(FPoint a)
	{
		return (a.X * a.X + a.Y * a.Y);
	}

	FORCEINLINE static FPoint Min(FPoint lhs, FPoint rhs)
	{
		return FPoint(FMath::Min(lhs.X, rhs.X), FMath::Min(lhs.Y, rhs.Y));
	}

	FORCEINLINE static FPoint Max(FPoint lhs, FPoint rhs)
	{
		return FPoint(FMath::Max(lhs.X, rhs.X), FMath::Max(lhs.Y, rhs.Y));
	}
};

FORCEINLINE uint32 GetTypeHash(FPoint& InPoint)
{
	return FCrc::MemCrc_DEPRECATED(&InPoint, sizeof(InPoint));
}
