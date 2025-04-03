// Fill out your copyright notice in the Description page of Project Settings.

#include "Math/MathTypes.h"

#include "Math/MathHelper.h"

const FIndex FIndex::ZeroIndex = FIndex(0, 0, 0);
const FIndex FIndex::OneIndex = FIndex(1, 1, 1);

FIndex::FIndex(int64 InValue)
{
	*this = FMathHelper::UnIndex(InValue);
}

int64 FIndex::ToInt64() const
{
	return FMathHelper::Index(*this);
}
