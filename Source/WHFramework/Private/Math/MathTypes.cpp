// Fill out your copyright notice in the Description page of Project Settings.

#include "Math/MathTypes.h"

#include "Math/MathStatics.h"

const FIndex FIndex::ZeroIndex = FIndex(0, 0, 0);
const FIndex FIndex::OneIndex = FIndex(1, 1, 1);

const FPoint FPoint::Zero = FPoint(0.f, 0.f);
const FPoint FPoint::One = FPoint(1.f, 1.f);
const FPoint FPoint::Up = FPoint(0.f, 1.f);
const FPoint FPoint::Right = FPoint(1.f, 0.f);

FIndex::FIndex(int64 InValue)
{
	*this = UMathStatics::UnIndex(InValue);
}

int64 FIndex::ToInt64() const
{
	return UMathStatics::Index(*this);
}
