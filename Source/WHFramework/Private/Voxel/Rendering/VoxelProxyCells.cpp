#include "Voxel/Rendering/VoxelProxyCells.h"

void FVoxelProxyCells::Init(const FVoxelBlockState InState, const int32 InCount)
{
	check(InCount >= 0);
	Count = InCount;
	Side = FMath::RoundToInt(FMath::Pow(double(InCount), 1.0 / 3.0));
	Interior = InState;
	Columns.Empty();
	Overrides.Empty();
	bSorted = true;
}

void FVoxelProxyCells::Add(const FVoxelBlockState InState)
{
	check(Columns.IsEmpty());
	const int32 Index = Count++;
	Set(Index, InState);
}

void FVoxelProxyCells::InitializeColumns(const int32 InSide, const FVoxelBlockState InInterior)
{
	check(InSide > 0 && InSide <= 256);
	Init(InInterior, InSide * InSide * InSide);
	Side = InSide;
	Columns.Reserve(Side * Side);
	Columns.SetNum(Side * Side);
}

void FVoxelProxyCells::SetColumn(const int32 InIndex, const FVoxelProxyColumn& InColumn)
{
	check(Columns.IsValidIndex(InIndex));
	Columns[InIndex] = InColumn;
}

int32 FVoxelProxyCells::LowerBound(const int32 InIndex) const
{
	check(bSorted);
	int32 First = 0;
	int32 Remaining = Overrides.Num();
	while (Remaining > 0)
	{
		const int32 Half = Remaining / 2;
		const int32 Middle = First + Half;
		if (Overrides[Middle].Index < InIndex) { First = Middle + 1; Remaining -= Half + 1; }
		else Remaining = Half;
	}
	return First;
}

FVoxelBlockState FVoxelProxyCells::GetNatural(const int32 InIndex) const
{
	if (Columns.IsEmpty()) return Interior;
	const int32 Area = Side * Side;
	const auto& Column = Columns[InIndex % Area];
	const int32 Z = InIndex / Area;
	if (Z < Column.Ground) return Interior;
	if (Z == Column.Ground) return Column.Surface;
	return Z <= Column.Water ? Column.WaterState : FVoxelBlockState();
}

FVoxelBlockState FVoxelProxyCells::Get(const int32 InIndex) const
{
	check(IsValidIndex(InIndex));
	const int32 At = LowerBound(InIndex);
	return Overrides.IsValidIndex(At) && Overrides[At].Index == InIndex ? Overrides[At].State : GetNatural(InIndex);
}

void FVoxelProxyCells::ReserveOverrides(const int32 InCount)
{
	if (InCount <= Overrides.Max()) return;
	const int32 Capacity = int32(FMath::Min<int64>(Count, FMath::Max<int64>(InCount, FMath::Max<int64>(16, int64(Overrides.Max()) * 2))));
	Overrides.Reserve(Capacity);
}

void FVoxelProxyCells::Set(const int32 InIndex, const FVoxelBlockState InState)
{
	check(IsValidIndex(InIndex));
	const int32 At = LowerBound(InIndex);
	const bool bExists = Overrides.IsValidIndex(At) && Overrides[At].Index == InIndex;
	if (InState == GetNatural(InIndex))
	{
		if (bExists) Overrides.RemoveAt(At, 1, EAllowShrinking::No);
	}
	else if (bExists) Overrides[At].State = InState;
	else
	{
		ReserveOverrides(Overrides.Num() + 1);
		Overrides.Insert({InIndex, InState}, At);
	}
}

void FVoxelProxyCells::AppendShard(const FVoxelProxyCells& InShard, const FIntVector InOffset)
{
	check(InShard.Validate(InShard.Side) && InShard.Columns.Num() == InShard.Side * InShard.Side && Columns.Num() == Side * Side && Interior == InShard.Interior);
	for (int32 Y = 0; Y < InShard.Side; ++Y)
		for (int32 X = 0; X < InShard.Side; ++X)
		{
			auto Column = InShard.Columns[X + Y * InShard.Side];
			Column.Ground += InOffset.Z;
			Column.Water += InOffset.Z;
			Columns[InOffset.X + X + Side * (InOffset.Y + Y)] = Column;
		}
	ReserveOverrides(Overrides.Num() + InShard.Overrides.Num());
	for (const auto& Cell : InShard.Overrides)
	{
		const int32 X = Cell.Index % InShard.Side + InOffset.X;
		const int32 Y = Cell.Index / InShard.Side % InShard.Side + InOffset.Y;
		const int32 Z = Cell.Index / (InShard.Side * InShard.Side) + InOffset.Z;
		Overrides.Add({X + Side * (Y + Side * Z), Cell.State});
	}
	bSorted &= InShard.Overrides.IsEmpty();
}

void FVoxelProxyCells::Sort()
{
	if (!bSorted) Overrides.Sort([](const auto& A, const auto& B) { return A.Index < B.Index; });
	bSorted = true;
}

uint64 FVoxelProxyCells::GetAllocatedSize() const
{
	return Columns.GetAllocatedSize() + Overrides.GetAllocatedSize();
}

bool FVoxelProxyCells::Validate(const int32 InSide) const
{
	if (InSide <= 0 || InSide > 256 || Count != InSide * InSide * InSide ||
		(!Columns.IsEmpty() && (Side != InSide || Columns.Num() != InSide * InSide)) || !bSorted) return false;
	return true;
}

bool FVoxelProxyCells::ContainsByPredicate(TFunctionRef<bool(const FVoxelBlockState&)> InPredicate) const
{
	int64 Matches = 0;
	if (Columns.IsEmpty()) Matches = InPredicate(Interior) ? Count : 0;
	else for (const auto& Column : Columns)
	{
		const int32 GroundEnd = FMath::Clamp(Column.Ground, 0, Side);
		const int32 WaterStart = FMath::Clamp<int64>(int64(Column.Ground) + 1, 0, Side);
		const int32 WaterEnd = FMath::Max(WaterStart, int32(FMath::Clamp<int64>(int64(Column.Water) + 1, 0, Side)));
		const int32 SurfaceCount = Column.Ground >= 0 && Column.Ground < Side ? 1 : 0;
		if (InPredicate(Interior)) Matches += GroundEnd;
		if (SurfaceCount && InPredicate(Column.Surface)) ++Matches;
		if (InPredicate(Column.WaterState)) Matches += WaterEnd - WaterStart;
		if (InPredicate(FVoxelBlockState())) Matches += Side - GroundEnd - SurfaceCount - (WaterEnd - WaterStart);
	}
	for (const auto& Cell : Overrides)
	{
		Matches -= InPredicate(GetNatural(Cell.Index));
		Matches += InPredicate(Cell.State);
	}
	return Matches > 0;
}

void FVoxelProxyCells::Transform(TFunctionRef<void(FVoxelBlockState&)> InTransform)
{
	InTransform(Interior);
	for (auto& Column : Columns) { InTransform(Column.Surface); InTransform(Column.WaterState); }
	for (auto& Cell : Overrides) InTransform(Cell.State);
}

bool FVoxelProxyCells::MayHaveSurface(const FIntVector InOffset, const int32 InSide) const
{
	if (Columns.IsEmpty()) return !Overrides.IsEmpty() || !Interior.IsAir();
	int32 GroundMin = MAX_int32;
	int32 GroundMax = MIN_int32;
	int32 WaterMax = MIN_int32;
	int32 WaterMin = MAX_int32;
	for (int32 Y = FMath::Max(0, InOffset.Y - 1); Y < FMath::Min(Side, InOffset.Y + InSide + 1); ++Y)
		for (int32 X = FMath::Max(0, InOffset.X - 1); X < FMath::Min(Side, InOffset.X + InSide + 1); ++X)
		{
			const auto& Column = Columns[X + Y * Side];
			GroundMin = FMath::Min(GroundMin, Column.Ground);
			GroundMax = FMath::Max(GroundMax, Column.Ground);
			WaterMax = FMath::Max(WaterMax, Column.Water);
			WaterMin = FMath::Min(WaterMin, Column.Water);
		}
	if ((int64(InOffset.Z) <= int64(GroundMax) + 1 && int64(InOffset.Z) + InSide > GroundMin) ||
		(int64(InOffset.Z) <= int64(WaterMax) + 1 && int64(InOffset.Z) + InSide > WaterMin)) return true;
	for (const auto& Cell : Overrides)
	{
		const FIntVector Local(Cell.Index % Side, Cell.Index / Side % Side, Cell.Index / (Side * Side));
		if (Local.X >= InOffset.X - 1 && Local.X <= InOffset.X + InSide &&
			Local.Y >= InOffset.Y - 1 && Local.Y <= InOffset.Y + InSide &&
			Local.Z >= InOffset.Z - 1 && Local.Z <= InOffset.Z + InSide) return true;
	}
	return false;
}

bool FVoxelProxyCells::Import(const int32 InSide, const FVoxelBlockState InInterior,
	TArray<FVoxelProxyColumn>&& InColumns, TArray<FVoxelProxyCell>&& InOverrides)
{
	FVoxelProxyCells Candidate;
	Candidate.Init(InInterior, InSide > 0 && InSide <= 256 ? InSide * InSide * InSide : 0);
	Candidate.Side = InSide;
	Candidate.Columns = MoveTemp(InColumns);
	Candidate.Overrides = MoveTemp(InOverrides);
	if (!Candidate.Validate(InSide)) return false;
	int32 Previous = -1;
	for (const auto& Cell : Candidate.Overrides)
	{
		if (Cell.Index <= Previous || !Candidate.IsValidIndex(Cell.Index)) return false;
		Previous = Cell.Index;
	}
	*this = MoveTemp(Candidate);
	return true;
}
