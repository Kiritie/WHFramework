#include "Voxel/Streaming/VoxelFineResidency.h"

#include "Voxel/Generation/VoxelGenerationAllocation.h"

void FVoxelFineResidency::BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
{
	Resources = InResources;
	Consumer = InConsumer;
}

TSharedPtr<const FVoxelFineResidency, ESPMode::ThreadSafe> FVoxelFineResidency::Capture(FString& OutError) const
{
	if (!Resources) return MakeShared<FVoxelFineResidency, ESPMode::ThreadSafe>(*this);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineResidency>(), Lease))
	{
		OutError = TEXT("CapacityBlocked: Fine residency snapshot");
		return nullptr;
	}
	return VoxelBoundedAllocation::MakeWorkerShared<FVoxelFineResidency>(Lease, *this);
}

bool FVoxelFineResidency::Initialize(const FGuid& InSource, const FVoxelFineWindowSettings& InSettings,
	const FIntVector& InPosition, const int32 InCoreRadiusCells, const int32 InCoreVerticalCells,
	const int32 InGuardRadiusCells, const int32 InWorldMinZ, const int32 InWorldMaxZ, FString& OutError)
{
	if (!InSource.IsValid() || InCoreVerticalCells <= 0 ||
		int64(InCoreVerticalCells) + 8 + InSettings.GuardMarginCells + InSettings.VerticalShiftStepCells >= InSettings.VerticalHalfExtentCells ||
		!InSettings.Validate(InCoreRadiusCells, InGuardRadiusCells, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Fine Source 身份或 F0 竖向范围不合法");
		}
		return false;
	}
	FVoxelFineResidency Candidate;
	Candidate.Resources = Resources;
	Candidate.Consumer = Consumer;
	Candidate.Source = InSource;
	Candidate.Settings = InSettings;
	Candidate.CoreRadiusCells = InCoreRadiusCells;
	Candidate.CoreVerticalCells = InCoreVerticalCells;
	Candidate.GuardRadiusCells = InGuardRadiusCells;
	Candidate.WorldMinZ = InWorldMinZ;
	Candidate.WorldMaxZ = InWorldMaxZ;
	Candidate.NextWindowSerial = NextWindowSerial + 1;
	FVoxelFineWindow NewWindow;
	FVoxelFineSpatialCapacity WindowCapacity{Resources, Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineWindow>()};
	if (Candidate.NextWindowSerial == 0 ||
		!FVoxelFineSpatial::BuildWindow(InPosition, InSettings, InWorldMinZ, InWorldMaxZ,
			Candidate.NextWindowSerial, NewWindow, OutError, &WindowCapacity))
	{
		return false;
	}
	Candidate.Current = WindowCapacity.Lease ? VoxelBoundedAllocation::MakeWorkerShared<FVoxelFineWindow>(WindowCapacity.Lease, MoveTemp(NewWindow)) :
		MakeShared<FVoxelFineWindow, ESPMode::ThreadSafe>(MoveTemp(NewWindow));
	FVoxelFineMovementDelta Movement;
	if (!Candidate.UpdateMovement(InPosition, FVector::ZeroVector, 0.0, Movement, OutError, false))
	{
		return false;
	}
	Candidate.ResidencyRevision = ResidencyRevision + 1;
	*this = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

bool FVoxelFineResidency::UpdateMovement(const FIntVector& InPosition, const FVector& InVelocityCellsPerSecond,
	const double InPredictionSeconds, FVoxelFineMovementDelta& OutDelta, FString& OutError, const bool bInProduceDelta)
{
	if (GetCurrent().Serial == 0 || InVelocityCellsPerSecond.ContainsNaN() ||
		!FMath::IsFinite(InPredictionSeconds) || InPredictionSeconds < 0.0)
	{
		OutError = TEXT("Fine 移动输入不合法或尚未初始化");
		return false;
	}
	FIntVector NewCoreAnchor;
	if (!FVoxelFineSpatial::TrySnap(InPosition, 16, NewCoreAnchor))
	{
		OutError = TEXT("Fine Core Anchor 越界");
		return false;
	}
	NewCoreAnchor += FIntVector(8);
	const FVector Predicted = FVector(InPosition) + InVelocityCellsPerSecond * InPredictionSeconds;
	if (Predicted.GetMin() < MIN_int32 + 32.0 || Predicted.GetMax() > MAX_int32 - 32.0)
	{
		OutError = TEXT("预测位置越界");
		return false;
	}
	FIntVector NewGuardEnd;
	if (!FVoxelFineSpatial::TrySnap(FIntVector(FMath::FloorToInt(Predicted.X), FMath::FloorToInt(Predicted.Y),
		FMath::FloorToInt(Predicted.Z)), 16, NewGuardEnd))
	{
		OutError = TEXT("预测 Guard Anchor 越界");
		return false;
	}
	NewGuardEnd += FIntVector(8);
	FVoxelFineMovementDelta Candidate;
	Candidate.bCoreChanged = !CoreAnchor.IsSet() || CoreAnchor.GetValue() != NewCoreAnchor;
	Candidate.bGuardChanged = Candidate.bCoreChanged || !GuardEndAnchor.IsSet() || GuardEndAnchor.GetValue() != NewGuardEnd;
	Candidate.bPriorityChanged = LastPosition != InPosition || LastVelocity != InVelocityCellsPerSecond;
	TSet<FIntVector> NewCore;
	TSet<FIntVector> NewGuard;
	FVoxelFineSpatialCapacity CoreCapacity{Resources, Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<TSet<FIntVector>>()};
	FVoxelFineSpatialCapacity GuardCapacity = CoreCapacity;
	if (Candidate.bCoreChanged)
	{
		if (!FVoxelFineSpatial::GatherCore(NewCoreAnchor, CoreRadiusCells + 12, CoreVerticalCells + 8,
			WorldMinZ, WorldMaxZ, NewCore, OutError, &CoreCapacity))
		{
			return false;
		}
		if (bInProduceDelta && !TVoxelFineSetDelta<FIntVector>::TryCalculate(GetCore(), NewCore,
			Candidate.F0, OutError, Resources, Consumer))
		{
			return false;
		}
	}
	if (Candidate.bGuardChanged)
	{
		if (!FVoxelFineSpatial::GatherSweptGuard(FVector(NewCoreAnchor), FVector(NewGuardEnd), GuardRadiusCells + 14,
			WorldMinZ, WorldMaxZ, 262144, NewGuard, OutError, &GuardCapacity))
		{
			return false;
		}
		if (bInProduceDelta && !TVoxelFineSetDelta<FIntVector>::TryCalculate(GetGuard(), NewGuard,
			Candidate.Collision, OutError, Resources, Consumer))
		{
			return false;
		}
	}
	if (Candidate.bCoreChanged)
	{
		Core = CoreCapacity.Lease ? VoxelBoundedAllocation::MakeWorkerShared<TSet<FIntVector>>(CoreCapacity.Lease, MoveTemp(NewCore)) :
			MakeShared<TSet<FIntVector>, ESPMode::ThreadSafe>(MoveTemp(NewCore));
		CoreAnchor = NewCoreAnchor;
	}
	if (Candidate.bGuardChanged)
	{
		Guard = GuardCapacity.Lease ? VoxelBoundedAllocation::MakeWorkerShared<TSet<FIntVector>>(GuardCapacity.Lease, MoveTemp(NewGuard)) :
			MakeShared<TSet<FIntVector>, ESPMode::ThreadSafe>(MoveTemp(NewGuard));
		GuardEndAnchor = NewGuardEnd;
	}
	if (Candidate.bCoreChanged || Candidate.bGuardChanged)
	{
		++RuntimeDemandRevision;
	}
	if (Candidate.bPriorityChanged || PriorityRevision == 0)
	{
		++PriorityRevision;
	}
	LastPosition = InPosition;
	LastVelocity = InVelocityCellsPerSecond;
	LastPredictionSeconds = InPredictionSeconds;
	OutDelta = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

bool FVoxelFineResidency::GatherSafetySweep(const FVoxelFineSafetyContext& InSafetyContext,
	TSet<FIntVector>& OutSweep, FString& OutError, FVoxelFineSpatialCapacity* InCapacity) const
{
	if (GetCurrent().Serial == 0 || InSafetyContext.WorldEpoch == 0 || InSafetyContext.CoverageRootSerial == 0 ||
		!FMath::IsFinite(InSafetyContext.PredictionSeconds) || InSafetyContext.PredictionSeconds < 0.0 ||
		InSafetyContext.PredictionSeconds != LastPredictionSeconds)
	{
		OutError = TEXT("安全查询缺少一致覆盖根或移动预测版本不一致");
		return false;
	}
	const FVector Proposed(LastPosition);
	const FVector Previous(InSafetyContext.LastAcceptedPosition);
	const FVector Future = Proposed + LastVelocity * InSafetyContext.PredictionSeconds;
	const double HorizontalExtent = FMath::Max(CoreRadiusCells + 12, GuardRadiusCells + 14);
	const double VerticalExtent = FMath::Max(CoreVerticalCells + 8, GuardRadiusCells + 14);
	FIntVector First;
	FIntVector Last;
	int64 SectionCount = 1;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const double Extent = Axis == 2 ? VerticalExtent : HorizontalExtent;
		double Minimum = FMath::Min(FMath::Min(Previous[Axis], Proposed[Axis]), Future[Axis]) - Extent;
		double Maximum = FMath::Max(FMath::Max(Previous[Axis], Proposed[Axis]), Future[Axis]) + Extent + 1.0;
		if (Axis == 2)
		{
			Minimum = FMath::Max(Minimum, double(WorldMinZ));
			Maximum = FMath::Min(Maximum, double(WorldMaxZ));
		}
		if (!FMath::IsFinite(Minimum) || !FMath::IsFinite(Maximum) || Minimum < MIN_int32 ||
			Maximum > MAX_int32 || Minimum >= Maximum)
		{
			OutError = TEXT("安全扫掠越界或为空");
			return false;
		}
		const int64 MinimumCell = FMath::FloorToInt64(Minimum);
		const int64 MaximumCell = FMath::CeilToInt64(Maximum);
		First[Axis] = int32(FVoxelFineSpatial::FloorDivide(MinimumCell, 16));
		Last[Axis] = int32(FVoxelFineSpatial::FloorDivide(MaximumCell - 1, 16));
		const int64 Count = int64(Last[Axis]) - First[Axis] + 1;
		if (Count > 262144 / SectionCount)
		{
			OutError = TEXT("SafetyRangeTooLarge：安全扫掠超过受控查询上限");
			return false;
		}
		SectionCount *= Count;
	}
	TSet<FIntVector> Candidate;
	if (InCapacity && !InCapacity->GrantSet<FIntVector>(int32(SectionCount), OutError)) return false;
	Candidate.Reserve(int32(SectionCount));
	for (int32 SectionZ = First.Z; SectionZ <= Last.Z; ++SectionZ)
	{
		for (int32 SectionY = First.Y; SectionY <= Last.Y; ++SectionY)
		{
			for (int32 SectionX = First.X; SectionX <= Last.X; ++SectionX)
			{
				Candidate.Add(FIntVector(SectionX, SectionY, SectionZ));
			}
		}
	}
	OutSweep = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

bool FVoxelFineResidency::EvaluateSafety(const FVoxelFineSafetyContext& InSafetyContext,
	TFunctionRef<FVoxelFineSectionCoverage(const FIntVector&)> InCommittedCoverage,
	FVoxelFineSafetyResult& OutSafety, FString& OutError, const bool bInCollectMissing) const
{
	OutSafety = {};
	TSet<FIntVector> Sweep;
	FVoxelFineSpatialCapacity SweepCapacity{Resources, Consumer, EVoxelTerrainResourcePool::Working};
	if (!GatherSafetySweep(InSafetyContext, Sweep, OutError, &SweepCapacity))
	{
		return false;
	}
	FVoxelFineSafetyResult Candidate;
	Candidate.CoverageRootSerial = InSafetyContext.CoverageRootSerial;
	if (bInCollectMissing)
	{
		FVoxelFineSpatialCapacity MissingCapacity{Resources, Consumer, EVoxelTerrainResourcePool::Input};
		const int64 MaximumMissing64 = int64(GetCore().Num()) + GetGuard().Num() + Sweep.Num();
		if (MaximumMissing64 > MAX_int32)
		{
			OutError = TEXT("Oversized: Fine safety rows");
			return false;
		}
		const int32 MaximumMissing = int32(MaximumMissing64);
		if (!MissingCapacity.GrantSet<FIntVector>(MaximumMissing, OutError)) return false;
		Candidate.MissingSections.Reserve(MaximumMissing);
		Candidate.ResourceLease = MoveTemp(MissingCapacity.Lease);
	}
	const auto CoversAll = [&InSafetyContext, &InCommittedCoverage, &Candidate, bInCollectMissing](const TSet<FIntVector>& InSections)
	{
		bool bAllCovered = true;
		for (const FIntVector& Key : InSections)
		{
			const FVoxelFineSectionCoverage Evidence = InCommittedCoverage(Key);
			const bool bCovered = Evidence.WorldEpoch == InSafetyContext.WorldEpoch &&
					Evidence.CoverageRootSerial == InSafetyContext.CoverageRootSerial &&
					Evidence.bWholeVisualCommitted && Evidence.bWholeCollisionCommitted && Evidence.bContentVersionsCurrent;
			if (!bCovered)
			{
				if (bInCollectMissing) Candidate.MissingSections.Add(Key);
				bAllCovered = false;
			}
		}
		return bAllCovered;
	};
	Candidate.bWholeCoreCovered = CoversAll(GetCore());
	Candidate.bWholeGuardCovered = CoversAll(GetGuard());
	Candidate.bWholeSweepCovered = CoversAll(Sweep);
	OutSafety = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

bool FVoxelFineResidency::PrepareMigration(const FIntVector& InPosition, const FVector& InVelocityCellsPerSecond,
	const double InMeasuredPreparationSeconds, const FVoxelFineSafetyContext& InSafetyContext,
	TFunctionRef<FVoxelFineSectionCoverage(const FIntVector&)> InCommittedCoverage,
	FVoxelFineMigrationDecision& OutDecision, FString& OutError)
{
	FVoxelFineSafetyResult Safety;
	if (!EvaluateSafety(InSafetyContext, InCommittedCoverage, Safety, OutError) ||
		!PrepareWindow(InPosition, InVelocityCellsPerSecond, InMeasuredPreparationSeconds, OutDecision, OutError))
	{
		return false;
	}
	OutDecision.Safety = MoveTemp(Safety);
	OutDecision.bRequiresTravelBarrier = !OutDecision.Safety.IsSafe();
	return true;
}

bool FVoxelFineResidency::PrepareWindow(const FIntVector& InPosition, const FVector& InVelocityCellsPerSecond,
	const double InMeasuredPreparationSeconds, FVoxelFineMigrationDecision& OutDecision, FString& OutError, const bool bInProduceDelta)
{
	OutDecision = {};
	OutDecision.bRequiresTravelBarrier = true;
	if (LastPosition != InPosition || LastVelocity != InVelocityCellsPerSecond)
	{
		OutError = TEXT("必须先为同一移动提案更新Core和Guard");
		return false;
	}
	if (GetCurrent().Serial == 0 || InVelocityCellsPerSecond.ContainsNaN() || !FMath::IsFinite(InMeasuredPreparationSeconds))
	{
		OutError = TEXT("Fine Window 尚未初始化或准备时间无效");
		return false;
	}
	FVoxelFineMigrationDecision Decision;
	Decision.bRequiresTravelBarrier = true;
	const double Preparation = FMath::Clamp(InMeasuredPreparationSeconds,
		Settings.MinimumPreparationSeconds, Settings.MaximumPreparationSeconds);
	const double Speed = InVelocityCellsPerSecond.Size();
	Decision.RequiredMarginCells = FMath::Max(CoreRadiusCells + 12, GuardRadiusCells + 14) +
		Speed * Preparation + Settings.GuardMarginCells;
	const double RequiredVertical = FMath::Max(CoreVerticalCells + 8, GuardRadiusCells + 14) +
		FMath::Abs(InVelocityCellsPerSecond.Z) * Preparation + Settings.GuardMarginCells;
	if (GetPending().IsSet())
	{
		const auto ContainsSections = [this](const TSet<FIntVector>& InSections)
		{
			for (const auto& Key : InSections)
			{
				const FVoxelGenerationBounds Bounds{Key * 16, (Key + FIntVector(1)) * 16};
				if (!FVoxelFineSpatial::ContainsBounds(GetPending()->Bounds, Bounds))
				{
					return false;
				}
			}
			return true;
		};
		if (ContainsSections(GetCore()) && ContainsSections(GetGuard()))
		{
			OutDecision = MoveTemp(Decision);
			OutError.Reset();
			return true;
		}
	}
	FIntVector NewAnchor = GetCurrent().Anchor;
	bool bShouldMove = false;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const double Margin = Axis == 2 ? RequiredVertical : Decision.RequiredMarginCells;
		const double Position = InPosition[Axis];
		const bool bCanExtendNegative = Axis != 2 || GetCurrent().Bounds.Min.Z > WorldMinZ;
		const bool bCanExtendPositive = Axis != 2 || GetCurrent().Bounds.Max.Z < WorldMaxZ;
		const double NegativeRemaining = Position - GetCurrent().Bounds.Min[Axis];
		const double PositiveRemaining = GetCurrent().Bounds.Max[Axis] - Position;
		const bool bNearNegative = bCanExtendNegative && NegativeRemaining <= Margin;
		const bool bNearPositive = bCanExtendPositive && PositiveRemaining <= Margin;
		if (!bNearNegative && !bNearPositive)
		{
			continue;
		}
		int32 Direction = 0;
		if (bNearNegative && bNearPositive)
		{
			Direction = InVelocityCellsPerSecond[Axis] < 0.0 ? -1 : InVelocityCellsPerSecond[Axis] > 0.0 ? 1 :
				(NegativeRemaining < PositiveRemaining ? -1 : 1);
		}
		else
		{
			Direction = bNearNegative ? -1 : 1;
		}
		const int64 Coordinate = int64(NewAnchor[Axis]) + int64(Direction) * (Axis == 2 ? Settings.VerticalShiftStepCells : Settings.ShiftStepCells);
		if (Coordinate < MIN_int32 || Coordinate > MAX_int32)
		{
			OutError = TEXT("Pending Anchor 越界");
			return false;
		}
		NewAnchor[Axis] = int32(Coordinate);
		bShouldMove = true;
	}
	if (!bShouldMove)
	{
		if (GetPending().IsSet())
		{
			if (bInProduceDelta && !TVoxelFineSetDelta<FVoxelFineShardKey>::TryCalculate(
				GetResidentShards(), GetCurrent().Shards, Decision.Residency, OutError, Resources, Consumer))
			{
				return false;
			}
			Pending.Reset();
			Residents.Reset();
			++ResidencyRevision;
		}
		OutDecision = MoveTemp(Decision);
		OutError.Reset();
		return true;
	}
	if (GetPending().IsSet() && GetPending()->Anchor == NewAnchor)
	{
		OutDecision = MoveTemp(Decision);
		OutError.Reset();
		return true;
	}
	FVoxelFineWindow NewWindow;
	FVoxelFineSpatialCapacity WindowCapacity{Resources, Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<TOptional<FVoxelFineWindow>>()};
	const uint64 Serial = NextWindowSerial + 1;
	if (Serial == 0 || !FVoxelFineSpatial::BuildWindow(NewAnchor, Settings, WorldMinZ, WorldMaxZ, Serial, NewWindow, OutError, &WindowCapacity))
	{
		return false;
	}
	FVoxelFineSpatialCapacity ResidentsCapacity{Resources, Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<TSet<FVoxelFineShardKey>>()};
	const int64 MaximumResidents64 = int64(GetCurrent().Shards.Num()) + NewWindow.Shards.Num();
	if (MaximumResidents64 > MAX_int32)
	{
		OutError = TEXT("Oversized: Fine resident union");
		return false;
	}
	const int32 MaximumResidents = int32(MaximumResidents64);
	if (!ResidentsCapacity.GrantSet<FVoxelFineShardKey>(MaximumResidents, OutError)) return false;
	TSet<FVoxelFineShardKey> Combined;
	Combined.Reserve(MaximumResidents);
	Combined.Append(GetCurrent().Shards);
	Combined.Append(NewWindow.Shards);
	if (bInProduceDelta)
	{
		if (!TVoxelFineSetDelta<FVoxelFineShardKey>::TryCalculate(GetCurrent().Shards, NewWindow.Shards,
			Decision.WindowDifference, OutError, Resources, Consumer) ||
			!TVoxelFineSetDelta<FVoxelFineShardKey>::TryCalculate(GetResidentShards(), Combined,
			Decision.Residency, OutError, Resources, Consumer))
		{
			return false;
		}
	}
	Decision.bStartedPending = true;
	Pending = WindowCapacity.Lease ? VoxelBoundedAllocation::MakeWorkerShared<TOptional<FVoxelFineWindow>>(WindowCapacity.Lease, MoveTemp(NewWindow)) :
		MakeShared<TOptional<FVoxelFineWindow>, ESPMode::ThreadSafe>(MoveTemp(NewWindow));
	Residents = ResidentsCapacity.Lease ? VoxelBoundedAllocation::MakeWorkerShared<TSet<FVoxelFineShardKey>>(ResidentsCapacity.Lease, MoveTemp(Combined)) :
		MakeShared<TSet<FVoxelFineShardKey>, ESPMode::ThreadSafe>(MoveTemp(Combined));
	NextWindowSerial = Serial;
	++ResidencyRevision;
	OutDecision = MoveTemp(Decision);
	OutError.Reset();
	return true;
}

bool FVoxelFineResidency::CommitPending(const uint64 InExpectedSerial,
	TFunctionRef<FVoxelFineShardReadiness(const FVoxelFineShardKey&)> InReadiness,
	TFunctionRef<bool(const FIntVector&)> InCollisionReady,
	const FVoxelFineSafetyContext& InSafetyContext,
	TFunctionRef<FVoxelFineSectionCoverage(const FIntVector&)> InCommittedCoverage,
	TVoxelFineSetDelta<FVoxelFineShardKey>& OutDelta, FString& OutError, const bool bInProduceDelta)
{
	if (!GetPending().IsSet() || GetPending()->Serial != InExpectedSerial)
	{
		OutError = TEXT("Pending Window 身份已失效");
		return false;
	}
	if (!GetPending()->Bounds.Contains(LastPosition))
	{
		OutError = TEXT("玩家已离开 Pending 可服务范围，不能交接到过时窗口");
		return false;
	}
	for (const FIntVector& Key : GetCore())
	{
		const FVoxelGenerationBounds Bounds{Key * 16, (Key + FIntVector(1)) * 16};
		if (!FVoxelFineSpatial::ContainsBounds(GetPending()->Bounds, Bounds))
		{
			OutError = TEXT("Pending 未完整容纳当前 F0 核心区");
			return false;
		}
	}
	for (const FVoxelFineShardKey& Key : GetPending()->Shards)
	{
		const FVoxelFineShardReadiness Evidence = InReadiness(Key);
		if (!Evidence.bDataCurrent || !Evidence.bGeometryCurrent || !Evidence.bHighResolutionPresented)
		{
			OutError = TEXT("Pending Window 尚未形成当前版本的连续高分辨率覆盖");
			return false;
		}
	}
	for (const FIntVector& Key : GetGuard())
	{
		const FVoxelGenerationBounds Bounds{Key * 16, (Key + FIntVector(1)) * 16};
		if (!FVoxelFineSpatial::ContainsBounds(GetPending()->Bounds, Bounds))
		{
			OutError = TEXT("Pending 未完整容纳当前前向碰撞范围");
			return false;
		}
		if (!InCollisionReady(Key))
		{
			OutError = TEXT("Pending Window 的前向碰撞尚未提交");
			return false;
		}
	}
	FVoxelFineSafetyResult Safety;
	TSet<FIntVector> Sweep;
	FVoxelFineSpatialCapacity SweepCapacity{Resources, Consumer, EVoxelTerrainResourcePool::Working};
	if (!EvaluateSafety(InSafetyContext, InCommittedCoverage, Safety, OutError, false) || !Safety.IsSafe() ||
		!GatherSafetySweep(InSafetyContext, Sweep, OutError, &SweepCapacity))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("完整Core、Guard或连续扫掠缺少已提交视觉与碰撞");
		}
		return false;
	}
	for (const FIntVector& Key : Sweep)
	{
		const FVoxelGenerationBounds Bounds{Key * 16, (Key + FIntVector(1)) * 16};
		if (!FVoxelFineSpatial::ContainsBounds(GetPending()->Bounds, Bounds))
		{
			OutError = TEXT("Pending未完整容纳此次移动与预测的连续扫掠");
			return false;
		}
	}
	if (bInProduceDelta && !TVoxelFineSetDelta<FVoxelFineShardKey>::TryCalculate(GetResidentShards(), GetPending()->Shards,
		OutDelta, OutError, Resources, Consumer))
	{
		return false;
	}
	CurrentOptional = Pending;
	Current.Reset();
	Pending.Reset();
	Residents.Reset();
	++ResidencyRevision;
	OutError.Reset();
	return true;
}

TVoxelFineSetDelta<FVoxelFineShardKey> FVoxelFineResidency::AbandonPending()
{
	TVoxelFineSetDelta<FVoxelFineShardKey> Delta;
	FString Error;
	const bool bSuccess = TryAbandonPending(Delta, Error);
	checkf(bSuccess, TEXT("%s"), *Error);
	return Delta;
}

bool FVoxelFineResidency::TryAbandonPending(TVoxelFineSetDelta<FVoxelFineShardKey>& OutDelta,
	FString& OutError, const bool bInProduceDelta)
{
	if (bInProduceDelta && !TVoxelFineSetDelta<FVoxelFineShardKey>::TryCalculate(GetResidentShards(),
		GetCurrent().Shards, OutDelta, OutError, Resources, Consumer))
	{
		return false;
	}
	if (GetPending().IsSet())
	{
		Pending.Reset();
		Residents.Reset();
		++ResidencyRevision;
	}
	OutError.Reset();
	return true;
}

TSet<FVoxelFineShardKey> FVoxelFineResidency::ResidentShards() const
{
	return GetResidentShards();
}

const TSet<FVoxelFineShardKey>& FVoxelFineResidency::GetResidentShards() const
{
	return Residents ? *Residents : GetCurrent().Shards;
}

bool FVoxelFineResidency::Retains(const FVoxelFineShardKey& InKey) const
{
	return GetCurrent().Shards.Contains(InKey) || (GetPending().IsSet() && GetPending()->Shards.Contains(InKey));
}

const FVoxelFineWindow& FVoxelFineResidency::GetCurrent() const
{
	static const FVoxelFineWindow Empty;
	return Current ? *Current : CurrentOptional ? CurrentOptional->GetValue() : Empty;
}

const TOptional<FVoxelFineWindow>& FVoxelFineResidency::GetPending() const
{
	static const TOptional<FVoxelFineWindow> Empty;
	return Pending ? *Pending : Empty;
}

const TSet<FIntVector>& FVoxelFineResidency::GetCore() const
{
	static const TSet<FIntVector> Empty;
	return Core ? *Core : Empty;
}

const TSet<FIntVector>& FVoxelFineResidency::GetGuard() const
{
	static const TSet<FIntVector> Empty;
	return Guard ? *Guard : Empty;
}

uint64 FVoxelFineResidency::GetRuntimeDemandRevision() const
{
	return RuntimeDemandRevision;
}

uint64 FVoxelFineResidency::GetResidencyRevision() const
{
	return ResidencyRevision;
}

uint64 FVoxelFineResidency::GetPriorityRevision() const
{
	return PriorityRevision;
}

void FVoxelFineResidency::Reset()
{
	const uint64 PreviousSerial = NextWindowSerial;
	*this = {};
	NextWindowSerial = PreviousSerial;
}

uint64 FVoxelFineResidencyLeases::GetAllocatedBytes() const
{
	uint64 Bytes = VoxelGenerationAllocation::AllocatedBytes(Sources) + VoxelGenerationAllocation::AllocatedBytes(Counts);
	for (const auto& Pair : Sources) Bytes += VoxelGenerationAllocation::AllocatedBytes(Pair.Value);
	return Bytes;
}

TSharedPtr<FVoxelFineResidencyLeases, ESPMode::ThreadSafe> FVoxelFineResidencyLeases::Capture(
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer, FString& OutError) const
{
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineResidencyLeases>() + GetAllocatedBytes();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (InResources)
	{
		if (Bytes > InResources->Snapshot().Limits.InputBytes)
		{ OutError = TEXT("Oversized: Fine multi-Source lease root"); return nullptr; }
		if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input, Bytes, Lease))
		{ OutError = TEXT("CapacityBlocked: Fine multi-Source lease root"); return nullptr; }
	}
	auto Result = Lease ? VoxelBoundedAllocation::MakeWorkerShared<FVoxelFineResidencyLeases>(Lease, *this) :
		MakeShared<FVoxelFineResidencyLeases, ESPMode::ThreadSafe>(*this);
	Result->Resources = MoveTemp(InResources); Result->Consumer = InConsumer; Result->StorageLease = MoveTemp(Lease);
	check(Result->GetAllocatedBytes() <= GetAllocatedBytes());
	OutError.Reset();
	return Result;
}

bool FVoxelFineResidencyLeases::TrySetSource(const FGuid& InSource, const TSet<FVoxelFineShardKey>& InShards,
	TVoxelFineSetDelta<FVoxelFineShardKey>& OutDelta, FString& OutError)
{
	if (!InSource.IsValid()) { OutError = TEXT("Fine lease requires a stable Source"); return false; }
	const TSet<FVoxelFineShardKey>* Previous = Sources.Find(InSource);
	const int32 PreviousCount = Previous ? Previous->Num() : 0;
	const int64 MaximumCounts64 = int64(Counts.Num()) + InShards.Num();
	const int64 RetainedRows64 = int64(PreviousCount) + InShards.Num();
	if (MaximumCounts64 > MAX_int32 || RetainedRows64 > MAX_int32 || Sources.Num() == MAX_int32)
	{ OutError = TEXT("Oversized: Fine multi-Source lease rows"); return false; }
	const uint64 Metadata = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineResidencyLeases>();
	uint64 OtherSourceBytes = 0;
	for (const auto& Pair : Sources) if (Pair.Key != InSource) OtherSourceBytes += VoxelGenerationAllocation::AllocatedBytes(Pair.Value);
	const TSet<FVoxelFineShardKey> ReplacementShape;
	const uint64 NewBytes = Metadata + OtherSourceBytes +
		VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelFineShardKey>(ReplacementShape, InShards.Num()) +
		VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FGuid, TSet<FVoxelFineShardKey>>>(Sources, Sources.Num() + 1) +
		VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FVoxelFineShardKey, int32>>(Counts, int32(MaximumCounts64));
	TVoxelFineSetDelta<FVoxelFineShardKey> Global;
	if (Resources)
	{
		const uint64 Peak = Metadata + GetAllocatedBytes() + NewBytes;
		const uint64 DeltaBytes = VoxelBoundedAllocation::SetBytes<FVoxelFineShardKey>(InShards.Num()) +
			VoxelBoundedAllocation::SetBytes<FVoxelFineShardKey>(PreviousCount) +
			VoxelBoundedAllocation::SetBytes<FVoxelFineShardKey>(int32(RetainedRows64));
		const auto Limits = Resources->Snapshot().Limits;
		if (Peak > Limits.InputBytes || DeltaBytes > Limits.InputBytes)
		{ OutError = TEXT("Oversized: Fine multi-Source lease allocation peak"); return false; }
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
			FMath::Max<uint64>(1, DeltaBytes), Global.ResourceLease) ||
			(!StorageLease && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Peak, StorageLease)) ||
			!StorageLease->Resize(Peak))
		{ OutError = TEXT("CapacityBlocked: Fine multi-Source lease allocation peak"); return false; }
	}
	Global.Added.Reserve(InShards.Num()); Global.Removed.Reserve(PreviousCount); Global.Retained.Reserve(int32(RetainedRows64));
	TSet<FVoxelFineShardKey> Replacement;
	Replacement.Reserve(InShards.Num());
	for (const auto& Key : InShards) Replacement.Add(Key);
	Sources.Reserve(Sources.Num() + 1); Counts.Reserve(int32(MaximumCounts64));
	Previous = Sources.Find(InSource);
	for (const FVoxelFineShardKey& Key : InShards)
	{
		if (Previous && Previous->Contains(Key)) { Global.Retained.Add(Key); continue; }
		int32& Count = Counts.FindOrAdd(Key);
		check(Count < MAX_int32);
		if (Count++ == 0) Global.Added.Add(Key);
		else Global.Retained.Add(Key);
	}
	if (Previous) for (const FVoxelFineShardKey& Key : *Previous)
	{
		if (InShards.Contains(Key)) continue;
		int32& Count = Counts.FindChecked(Key);
		check(Count > 0);
		if (--Count == 0) { Counts.Remove(Key); Global.Removed.Add(Key); }
		else Global.Retained.Add(Key);
	}
	Sources.FindOrAdd(InSource) = MoveTemp(Replacement);
	check(GetAllocatedBytes() + Metadata <= NewBytes);
	if (StorageLease)
	{
		const bool bResized = StorageLease->Resize(FMath::Max<uint64>(1, NewBytes));
		check(bResized);
	}
	OutDelta = MoveTemp(Global);
	OutError.Reset();
	return true;
}

bool FVoxelFineResidencyLeases::TryRemoveSource(const FGuid& InSource,
	TVoxelFineSetDelta<FVoxelFineShardKey>& OutDelta, FString& OutError)
{
	if (!TrySetSource(InSource, {}, OutDelta, OutError)) return false;
	Sources.Remove(InSource);
	return true;
}

TVoxelFineSetDelta<FVoxelFineShardKey> FVoxelFineResidencyLeases::SetSource(const FGuid& InSource,
	const TSet<FVoxelFineShardKey>& InShards)
{
	TVoxelFineSetDelta<FVoxelFineShardKey> Delta;
	FString Error;
	const bool bSuccess = TrySetSource(InSource, InShards, Delta, Error);
	checkf(bSuccess, TEXT("%s"), *Error);
	return Delta;
}

TVoxelFineSetDelta<FVoxelFineShardKey> FVoxelFineResidencyLeases::RemoveSource(const FGuid& InSource)
{
	TVoxelFineSetDelta<FVoxelFineShardKey> Delta;
	FString Error;
	const bool bSuccess = TryRemoveSource(InSource, Delta, Error);
	checkf(bSuccess, TEXT("%s"), *Error);
	return Delta;
}
bool FVoxelFineResidencyLeases::IsRetained(const FVoxelFineShardKey& InKey) const
{
	return ReferenceCount(InKey) > 0;
}

int32 FVoxelFineResidencyLeases::ReferenceCount(const FVoxelFineShardKey& InKey) const
{
	const int32* Count = Counts.Find(InKey);
	return Count ? *Count : 0;
}

void FVoxelFineResidencyLeases::Reset()
{
	Sources.Reset();
	Counts.Reset();
}
