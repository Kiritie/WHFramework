#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	int64 VoxelGenerationLerp(
		int64 InA,
		int64 InB,
		int32 InAlphaQ16)
	{
		return InA +
			(InB - InA) *
			static_cast<int64>(InAlphaQ16) /
			65536;
	}

	int32 VoxelGenerationSmoothFraction(
		int32 InPosition,
		int32 InCell,
		int32 InPeriod)
	{
		const int64 Local =
			static_cast<int64>(InPosition) -
			static_cast<int64>(InCell) *
			InPeriod;

		const int64 Alpha =
			Local * 65536 /
			InPeriod;

		const int64 AlphaSquared =
			Alpha * Alpha /
			65536;

		const int64 Smooth =
			AlphaSquared *
			(196608 - 2 * Alpha) /
			65536;

		return static_cast<int32>(Smooth);
	}
}

int32 VoxelGeneration::FloorDivide(
	int32 InValue,
	int32 InDivisor)
{
	if (!ensureMsgf(
		InDivisor > 0,
		TEXT("VoxelGeneration::FloorDivide requires a positive divisor")))
	{
		return 0;
	}

	const int32 Quotient = InValue / InDivisor;
	const int32 Remainder = InValue % InDivisor;

	return Quotient - (Remainder < 0 ? 1 : 0);
}

int32 VoxelGeneration::CeilDividePositive(
	int32 InValue,
	int32 InDivisor)
{
	if (!ensureMsgf(
		InDivisor > 0,
		TEXT("VoxelGeneration::CeilDividePositive requires a positive divisor")))
	{
		return 0;
	}

	if (InValue <= 0)
	{
		return 0;
	}

	return static_cast<int32>(
		(static_cast<int64>(InValue) + InDivisor - 1) /
		InDivisor);
}

uint64 VoxelGeneration::Mix(uint64 InValue)
{
	uint64 Value = InValue + 0x9e3779b97f4a7c15ull;
	Value = (Value ^ (Value >> 30)) * 0xbf58476d1ce4e5b9ull;
	Value = (Value ^ (Value >> 27)) * 0x94d049bb133111ebull;
	return Value ^ (Value >> 31);
}

uint64 VoxelGeneration::MakeSeed(
	int32 InWorldSeed,
	const FIntVector& InAnchor,
	uint64 InSalt)
{
	uint64 Hash = Mix(static_cast<uint32>(InWorldSeed));
	Hash = Mix(Hash ^ static_cast<uint32>(InAnchor.X));
	Hash = Mix(Hash ^ static_cast<uint32>(InAnchor.Y));
	Hash = Mix(Hash ^ static_cast<uint32>(InAnchor.Z));
	return Mix(Hash ^ InSalt);
}

FVoxelStableId VoxelGeneration::MakeStableId(
	int32 InWorldSeed,
	const FIntVector& InAnchor,
	uint64 InKind,
	uint64 InOrdinal)
{
	const uint64 High =
		MakeSeed(InWorldSeed, InAnchor, InKind);

	FVoxelStableId Result;
	Result.High = High;
	Result.Low = Mix(High ^ InOrdinal);
	return Result;
}

int32 VoxelGeneration::RandomRange(
	uint64 InSeed,
	int32 InMin,
	int32 InMax)
{
	if (InMax <= InMin)
	{
		return InMin;
	}

	const int64 SignedWidth =
		static_cast<int64>(InMax) -
		static_cast<int64>(InMin) +
		1;

	if (SignedWidth <= 0)
	{
		return InMin;
	}

	return InMin +
		static_cast<int32>(
			InSeed %
			static_cast<uint64>(SignedWidth));
}

int32 VoxelGeneration::Noise2D(
	int32 InWorldSeed,
	int32 InX,
	int32 InY,
	int32 InPeriod,
	uint64 InSalt)
{
	return Noise3D(
		InWorldSeed,
		FIntVector(InX, InY, 0),
		InPeriod,
		InSalt);
}

int32 VoxelGeneration::Noise3D(
	int32 InWorldSeed,
	const FIntVector& InPosition,
	int32 InPeriod,
	uint64 InSalt)
{
	if (InPeriod <= 0)
	{
		return 0;
	}

	const FIntVector BaseCell(
		FloorDivide(InPosition.X, InPeriod),
		FloorDivide(InPosition.Y, InPeriod),
		FloorDivide(InPosition.Z, InPeriod));

	const int32 AlphaX =
		VoxelGenerationSmoothFraction(
			InPosition.X,
			BaseCell.X,
			InPeriod);

	const int32 AlphaY =
		VoxelGenerationSmoothFraction(
			InPosition.Y,
			BaseCell.Y,
			InPeriod);

	const int32 AlphaZ =
		VoxelGenerationSmoothFraction(
			InPosition.Z,
			BaseCell.Z,
			InPeriod);

	int64 InterpolatedZ[2] = {0, 0};

	for (int32 ZOffset = 0; ZOffset < 2; ++ZOffset)
	{
		int64 InterpolatedY[2] = {0, 0};

		for (int32 YOffset = 0; YOffset < 2; ++YOffset)
		{
			const FIntVector Cell0(
				BaseCell.X,
				BaseCell.Y + YOffset,
				BaseCell.Z + ZOffset);

			const FIntVector Cell1(
				BaseCell.X + 1,
				BaseCell.Y + YOffset,
				BaseCell.Z + ZOffset);

			const int64 Value0 =
				static_cast<int64>(
					MakeSeed(
						InWorldSeed,
						Cell0,
						InSalt) & 65535ull) -
				32768;

			const int64 Value1 =
				static_cast<int64>(
					MakeSeed(
						InWorldSeed,
						Cell1,
						InSalt) & 65535ull) -
				32768;

			InterpolatedY[YOffset] =
				VoxelGenerationLerp(
					Value0,
					Value1,
					AlphaX);
		}

		InterpolatedZ[ZOffset] =
			VoxelGenerationLerp(
				InterpolatedY[0],
				InterpolatedY[1],
				AlphaY);
	}

	return static_cast<int32>(
		VoxelGenerationLerp(
			InterpolatedZ[0],
			InterpolatedZ[1],
			AlphaZ));
}

int64 VoxelGeneration::Dot(
	const FIntVector& InA,
	const FIntVector& InB)
{
	return static_cast<int64>(InA.X) * InB.X +
		static_cast<int64>(InA.Y) * InB.Y +
		static_cast<int64>(InA.Z) * InB.Z;
}

bool VoxelGeneration::IsInsideEllipsoid(
	const FIntVector& InPosition,
	const FIntVector& InCenter,
	const FIntVector& InRadius)
{
	if (InRadius.X <= 0 ||
		InRadius.Y <= 0 ||
		InRadius.Z <= 0)
	{
		return false;
	}

	const int64 DeltaX =
		2ll * (static_cast<int64>(InPosition.X) - InCenter.X) + 1;

	const int64 DeltaY =
		2ll * (static_cast<int64>(InPosition.Y) - InCenter.Y) + 1;

	const int64 DeltaZ =
		2ll * (static_cast<int64>(InPosition.Z) - InCenter.Z) + 1;

	const int64 RadiusX = InRadius.X;
	const int64 RadiusY = InRadius.Y;
	const int64 RadiusZ = InRadius.Z;

	if (DeltaX < -2ll * RadiusX ||
		DeltaX > 2ll * RadiusX ||
		DeltaY < -2ll * RadiusY ||
		DeltaY > 2ll * RadiusY ||
		DeltaZ < -2ll * RadiusZ ||
		DeltaZ > 2ll * RadiusZ)
	{
		return false;
	}

	constexpr int64 Scale = 1ll << 24;

	const int64 XTerm =
		DeltaX * DeltaX * Scale /
		(4ll * RadiusX * RadiusX);

	const int64 YTerm =
		DeltaY * DeltaY * Scale /
		(4ll * RadiusY * RadiusY);

	const int64 ZTerm =
		DeltaZ * DeltaZ * Scale /
		(4ll * RadiusZ * RadiusZ);

	return XTerm + YTerm + ZTerm <= Scale;
}

bool VoxelGeneration::IsInsideCapsule(
	const FIntVector& InPosition,
	const FIntVector& InStart,
	const FIntVector& InEnd,
	int32 InRadius)
{
	if (InRadius <= 0)
	{
		return false;
	}

	const FIntVector Segment = InEnd - InStart;
	const FIntVector Relative = InPosition - InStart;

	const int64 SegmentLengthSquared = Dot(Segment, Segment);
	const int64 Projection = Dot(Relative, Segment);
	const int64 RadiusSquared =
		static_cast<int64>(InRadius) * InRadius;

	if (SegmentLengthSquared <= 0 ||
		Projection <= 0)
	{
		return Dot(Relative, Relative) <= RadiusSquared;
	}

	if (Projection >= SegmentLengthSquared)
	{
		const FIntVector EndRelative =
			InPosition - InEnd;

		return Dot(EndRelative, EndRelative) <= RadiusSquared;
	}

	return Dot(Relative, Relative) *
		SegmentLengthSquared -
		Projection * Projection <=
		RadiusSquared *
		SegmentLengthSquared;
}

uint64 VoxelGeneration::HashBytes(
	TConstArrayView<uint8> InBytes)
{
	uint64 Hash = 14695981039346656037ull;

	for (const uint8 Value : InBytes)
	{
		Hash ^= Value;
		Hash *= 1099511628211ull;
	}

	return Hash;
}
