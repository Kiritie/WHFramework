// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleBPLibrary.h"

EDirection UVoxelModuleBPLibrary::InvertDirection(EDirection InDirection)
{
	if ((int32)InDirection % 2 == 0)
		return (EDirection)((int32)InDirection + 1);
	else
		return (EDirection)((int32)InDirection - 1);
}

FVector UVoxelModuleBPLibrary::DirectionToVector(EDirection InDirection, FRotator InRotation /*= FRotator::ZeroRotator*/)
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

FIndex UVoxelModuleBPLibrary::DirectionToIndex(EDirection InDirection, FRotator InRotation /*= FRotator::ZeroRotator*/)
{
	return FIndex(DirectionToVector(InDirection, InRotation));
}

FIndex UVoxelModuleBPLibrary::GetAdjacentIndex(FIndex InIndex, EDirection InDirection, FRotator InRotation /*= FRotator::ZeroRotator*/)
{
	return InIndex + DirectionToIndex(InDirection, InRotation);
}
