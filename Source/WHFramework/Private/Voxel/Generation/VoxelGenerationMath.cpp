#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	uint64 EllipsoidTermQ24(const int64 InDelta, const int64 InRadius)
	{
		const uint64 AbsoluteDelta = uint64(FMath::Abs(InDelta));
		const uint64 Numerator = AbsoluteDelta * AbsoluteDelta;
		const uint64 Denominator = 4ull * uint64(InRadius) * uint64(InRadius);
		if (Numerator <= (MAX_uint64 >> 24)) return (Numerator << 24) / Denominator;
		// 极端半径仍保留原 Q24 整数取整语义，不构造会溢出的乘积。
		uint64 Result = Numerator / Denominator;
		uint64 Remainder = Numerator % Denominator;
		for (int32 Bit = 0; Bit < 24; ++Bit)
		{
			Result <<= 1;
			if (Remainder >= Denominator - Remainder)
			{
				Remainder -= Denominator - Remainder;
				++Result;
			}
			else Remainder *= 2;
		}
		return Result;
	}

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

int64 VoxelGeneration::FloorDivideWide(const int64 InValue, const int32 InDivisor)
{
	if (!ensureMsgf(InDivisor > 0, TEXT("VoxelGeneration::FloorDivideWide requires a positive divisor"))) return 0;
	return InValue / InDivisor - (InValue % InDivisor < 0 ? 1 : 0);
}

bool VoxelGeneration::IsWorldCell(const int64 InCoordinate)
{
	return InCoordinate >= WorldCellMin && InCoordinate < WorldCellMaxExclusive;
}

bool VoxelGeneration::TryWorldPoint(const FInt64Vector2& InCoordinate, FIntPoint& OutPoint)
{
	if (!IsWorldCell(InCoordinate.X) || !IsWorldCell(InCoordinate.Y)) return false;
	OutPoint = FIntPoint(int32(InCoordinate.X), int32(InCoordinate.Y));
	return true;
}

bool VoxelGeneration::TryWorldCell(const FInt64Vector& InCoordinate, FIntVector& OutCell)
{
	if (!IsWorldCell(InCoordinate.X) || !IsWorldCell(InCoordinate.Y) || !IsWorldCell(InCoordinate.Z)) return false;
	OutCell = FIntVector(int32(InCoordinate.X), int32(InCoordinate.Y), int32(InCoordinate.Z));
	return true;
}

bool VoxelGeneration::TryRoundedWorldCell(const FVector& InCoordinate, FIntVector& OutCell)
{
	// 先检查有限值及安全转换范围，不能先 RoundToInt 再检查已经溢出的结果。
	if (!FMath::IsFinite(InCoordinate.X) || !FMath::IsFinite(InCoordinate.Y) || !FMath::IsFinite(InCoordinate.Z) ||
		InCoordinate.X < double(WorldCellMin) - 1.0 || InCoordinate.X > double(WorldCellMaxExclusive) + 1.0 ||
		InCoordinate.Y < double(WorldCellMin) - 1.0 || InCoordinate.Y > double(WorldCellMaxExclusive) + 1.0 ||
		InCoordinate.Z < double(WorldCellMin) - 1.0 || InCoordinate.Z > double(WorldCellMaxExclusive) + 1.0) return false;
	return TryWorldCell(FInt64Vector(FMath::RoundToInt64(InCoordinate.X), FMath::RoundToInt64(InCoordinate.Y),
		FMath::RoundToInt64(InCoordinate.Z)), OutCell);
}

bool VoxelGeneration::ClipWorldBounds(const FInt64Vector& InMin, const FInt64Vector& InMax, FVoxelGenerationBounds& OutBounds)
{
	const FInt64Vector Min(FMath::Max(InMin.X, WorldCellMin), FMath::Max(InMin.Y, WorldCellMin), FMath::Max(InMin.Z, WorldCellMin));
	const FInt64Vector Max(FMath::Min(InMax.X, WorldCellMaxExclusive), FMath::Min(InMax.Y, WorldCellMaxExclusive),
		FMath::Min(InMax.Z, WorldCellMaxExclusive));
	if (Min.X >= Max.X || Min.Y >= Max.Y || Min.Z >= Max.Z) return false;
	OutBounds = {FIntVector(int32(Min.X), int32(Min.Y), int32(Min.Z)), FIntVector(int32(Max.X), int32(Max.Y), int32(Max.Z))};
	return true;
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

	return static_cast<int32>(static_cast<int64>(InMin) +
		static_cast<int64>(InSeed % static_cast<uint64>(SignedWidth)));
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
	const int32 ZSamples = AlphaZ == 0 ? 1 : 2;
	const int32 YSamples = AlphaY == 0 ? 1 : 2;

	for (int32 ZOffset = 0; ZOffset < ZSamples; ++ZOffset)
	{
		int64 InterpolatedY[2] = {0, 0};

		for (int32 YOffset = 0; YOffset < YSamples; ++YOffset)
		{
			// 格点是32位种子坐标，角点加法显式按无符号位模式进行，不受 world cell 域限制。
			const FIntVector Cell0(
				BaseCell.X,
				static_cast<int32>(static_cast<uint32>(BaseCell.Y) + uint32(YOffset)),
				static_cast<int32>(static_cast<uint32>(BaseCell.Z) + uint32(ZOffset)));

			const FIntVector Cell1(
				static_cast<int32>(static_cast<uint32>(BaseCell.X) + 1u),
				static_cast<int32>(static_cast<uint32>(BaseCell.Y) + uint32(YOffset)),
				static_cast<int32>(static_cast<uint32>(BaseCell.Z) + uint32(ZOffset)));

			const int64 Value0 =
				static_cast<int64>(
					MakeSeed(
						InWorldSeed,
						Cell0,
						InSalt) & 65535ull) -
				32768;

			if (AlphaX == 0)
			{
				InterpolatedY[YOffset] = Value0;
			}
			else
			{
				const int64 Value1 = static_cast<int64>(MakeSeed(InWorldSeed, Cell1, InSalt) & 65535ull) - 32768;
				InterpolatedY[YOffset] = VoxelGenerationLerp(Value0, Value1, AlphaX);
			}
		}

		InterpolatedZ[ZOffset] = AlphaY == 0 ? InterpolatedY[0] :
			VoxelGenerationLerp(InterpolatedY[0], InterpolatedY[1], AlphaY);
	}

	return static_cast<int32>(AlphaZ == 0 ? InterpolatedZ[0] :
		VoxelGenerationLerp(InterpolatedZ[0], InterpolatedZ[1], AlphaZ));
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
	return IsInsideEllipsoid(InPosition, FInt64Vector(InCenter), InRadius);
}

bool VoxelGeneration::IsInsideEllipsoid(
	const FIntVector& InPosition,
	const FInt64Vector& InCenter,
	const FIntVector& InRadius)
{
	if (InRadius.X <= 0 ||
		InRadius.Y <= 0 ||
		InRadius.Z <= 0)
	{
		return false;
	}
	// 宽几何中心可以在世界域外，先以安全包围范围排除远点再求差。
	if (InCenter.X < int64(InPosition.X) - InRadius.X || InCenter.X > int64(InPosition.X) + InRadius.X ||
		InCenter.Y < int64(InPosition.Y) - InRadius.Y || InCenter.Y > int64(InPosition.Y) + InRadius.Y ||
		InCenter.Z < int64(InPosition.Z) - InRadius.Z || InCenter.Z > int64(InPosition.Z) + InRadius.Z) return false;

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

	const uint64 XTerm = EllipsoidTermQ24(DeltaX, RadiusX);
	const uint64 YTerm = EllipsoidTermQ24(DeltaY, RadiusY);
	const uint64 ZTerm = EllipsoidTermQ24(DeltaZ, RadiusZ);

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

	const FInt64Vector WideSegment(int64(InEnd.X) - InStart.X,
		int64(InEnd.Y) - InStart.Y, int64(InEnd.Z) - InStart.Z);
	const FInt64Vector WideRelative(int64(InPosition.X) - InStart.X,
		int64(InPosition.Y) - InStart.Y, int64(InPosition.Z) - InStart.Z);
	const auto FitsExactProduct = [](const FInt64Vector& Vector)
	{
		return FMath::Abs(Vector.X) <= 16383 && FMath::Abs(Vector.Y) <= 16383 && FMath::Abs(Vector.Z) <= 16383;
	};
	if (InRadius <= 16383 && FitsExactProduct(WideSegment) && FitsExactProduct(WideRelative))
	{
		// 保留普通洞穴的整数边缘包含语义；此范围内所有四次乘积均可表示。
		const auto WideDot = [](const FInt64Vector& A, const FInt64Vector& B)
		{
			return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
		};
		const int64 LengthSquared = WideDot(WideSegment, WideSegment);
		const int64 Projection = WideDot(WideRelative, WideSegment);
		const int64 RadiusSquared = int64(InRadius) * InRadius;
		if (LengthSquared == 0 || Projection <= 0) return WideDot(WideRelative, WideRelative) <= RadiusSquared;
		if (Projection >= LengthSquared)
		{
			const FInt64Vector EndRelative = WideRelative - WideSegment;
			return WideDot(EndRelative, EndRelative) <= RadiusSquared;
		}
		return WideDot(WideRelative, WideRelative) * LengthSquared - Projection * Projection <= RadiusSquared * LengthSquared;
	}
	// 全域差值与四次乘积可能超出整数容器；最近点残差避免巨大四次项相减。
	const FVector Segment(double(WideSegment.X), double(WideSegment.Y), double(WideSegment.Z));
	const FVector Relative(double(WideRelative.X), double(WideRelative.Y), double(WideRelative.Z));
	const double SegmentLengthSquared = Segment.SizeSquared();
	const double Alpha = SegmentLengthSquared > 0.0 ?
		FMath::Clamp(FVector::DotProduct(Relative, Segment) / SegmentLengthSquared, 0.0, 1.0) : 0.0;
	return (Relative - Segment * Alpha).SizeSquared() <= double(InRadius) * InRadius;
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
