#include "Ability/Combat/AbilityHitQuery.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

namespace
{
	bool BuildQuery(FAbilityHitQueryContext& Context, FCollisionQueryParams& Query, FCollisionObjectQueryParams& Objects)
	{
		if (!Context.World || Context.RemainingQueries <= 0 || Context.TargetObjectChannels.IsEmpty() ||
			Context.BlockingChannel >= ECC_MAX || !Context.IsTargetActor)
		{
			return false;
		}
		for (ECollisionChannel Channel : Context.TargetObjectChannels)
		{
			if (Channel >= ECC_MAX)
			{
				return false;
			}
			Objects.AddObjectTypesToQuery(Channel);
		}
		for (const TWeakObjectPtr<AActor>& Actor : Context.IgnoredActors)
		{
			if (Actor.IsValid())
			{
				Query.AddIgnoredActor(Actor.Get());
			}
		}
		Query.bFindInitialOverlaps = true;
		Query.bReturnPhysicalMaterial = true;
		return true;
	}

	FTransform InterpolateFrame(const FTransform& A, const FTransform& B, float Alpha)
	{
		return FTransform(FQuat::Slerp(A.GetRotation(), B.GetRotation(), Alpha).GetNormalized(),
			FMath::Lerp(A.GetLocation(), B.GetLocation(), Alpha), FVector::OneVector);
	}
}

bool FAbilityHitShape::IsValid() const
{
	if (!FMath::IsFinite(Radius) || !FMath::IsFinite(HalfHeight) || HalfExtent.ContainsNaN())
	{
		return false;
	}
	switch (Type)
	{
	case EAbilityHitShape::Sphere:
		return Radius > 0.f && Radius <= 10000.f;
	case EAbilityHitShape::Capsule:
		return Radius > 0.f && HalfHeight >= Radius && HalfHeight <= 10000.f;
	case EAbilityHitShape::Box:
		return HalfExtent.GetMin() > 0.f && HalfExtent.GetMax() <= 10000.f;
	default:
		return false;
	}
}

FCollisionShape FAbilityHitShape::MakeCollisionShape() const
{
	switch (Type)
	{
	case EAbilityHitShape::Capsule:
		return FCollisionShape::MakeCapsule(Radius, HalfHeight);
	case EAbilityHitShape::Box:
		return FCollisionShape::MakeBox(HalfExtent);
	default:
		return FCollisionShape::MakeSphere(Radius);
	}
}

float FAbilityHitShape::GetBoundingRadius() const
{
	if (Type == EAbilityHitShape::Box)
	{
		return HalfExtent.Size();
	}
	return Type == EAbilityHitShape::Capsule ? HalfHeight : Radius;
}

bool FAbilityHitQueryDefinition::Validate(float MaximumRange, FString& OutError) const
{
	if (Mode == EAbilityHitQueryMode::None || LocalStart.ContainsNaN() || LocalEnd.ContainsNaN() ||
		LocalRotation.ContainsNaN() || !FMath::IsFinite(MagnitudeScale) || MagnitudeScale < 0.f ||
		HitGroup < 0 || HitGroup > 63 || !FMath::IsFinite(MaximumRange) || MaximumRange <= 0.f)
	{
		OutError = TEXT("命中查询类型、坐标、倍率或去重组不合法");
		return false;
	}
	if (Mode != EAbilityHitQueryMode::Ray && !Shape.IsValid())
	{
		OutError = TEXT("命中查询形状不合法");
		return false;
	}
	const float Padding = Mode == EAbilityHitQueryMode::Ray ? 0.f : Shape.GetBoundingRadius();
	if (LocalStart.Size() + Padding > MaximumRange || LocalEnd.Size() + Padding > MaximumRange)
	{
		OutError = TEXT("命中查询超出技能最大范围");
		return false;
	}
	if (Mode == EAbilityHitQueryMode::Trajectory)
	{
		if (Shape.Type != EAbilityHitShape::Sphere || Trajectory.Num() < 2 || Trajectory.Num() > 32 ||
			!FMath::IsNearlyZero(Trajectory[0].Alpha) || !FMath::IsNearlyEqual(Trajectory.Last().Alpha, 1.f))
		{
			OutError = TEXT("连续近战轨迹必须使用球形厚度并覆盖0到1，关键帧数量为2到32");
			return false;
		}
		float Previous = -1.f;
		for (const FAbilityHitTrajectoryKey& Key : Trajectory)
		{
			if (!FMath::IsFinite(Key.Alpha) || Key.Alpha <= Previous || Key.Alpha < 0.f || Key.Alpha > 1.f ||
				Key.LocalStart.ContainsNaN() || Key.LocalEnd.ContainsNaN() ||
				Key.LocalStart.Size() + Shape.Radius > MaximumRange || Key.LocalEnd.Size() + Shape.Radius > MaximumRange ||
				FVector::Distance(Key.LocalStart, Key.LocalEnd) > Shape.Radius * 32.f)
			{
				OutError = TEXT("连续近战关键帧时间、范围或刀刃长度不合法");
				return false;
			}
			Previous = Key.Alpha;
		}
	}
	return true;
}

void FAbilityHitQueryDefinition::EvaluateTrajectory(float Alpha, FVector& OutStart, FVector& OutEnd) const
{
	if (Trajectory.IsEmpty())
	{
		OutStart = LocalStart;
		OutEnd = LocalEnd;
		return;
	}
	Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
	for (int32 Index = 1; Index < Trajectory.Num(); ++Index)
	{
		if (Alpha <= Trajectory[Index].Alpha)
		{
			const FAbilityHitTrajectoryKey& A = Trajectory[Index - 1];
			const FAbilityHitTrajectoryKey& B = Trajectory[Index];
			const float Weight = (Alpha - A.Alpha) / FMath::Max(UE_SMALL_NUMBER, B.Alpha - A.Alpha);
			OutStart = FMath::Lerp(A.LocalStart, B.LocalStart, Weight);
			OutEnd = FMath::Lerp(A.LocalEnd, B.LocalEnd, Weight);
			return;
		}
	}
	OutStart = Trajectory.Last().LocalStart;
	OutEnd = Trajectory.Last().LocalEnd;
}

bool FAbilityHitQuery::Trace(FAbilityHitQueryContext& Context, const FVector& Start, const FVector& End,
	const FQuat& Rotation, const FAbilityHitShape* Shape, TArray<FHitResult>& OutHits, FHitResult& OutBlocker)
{
	OutHits.Reset();
	OutBlocker = FHitResult();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(AbilityHitQuery), false);
	FCollisionObjectQueryParams Objects;
	if (Start.ContainsNaN() || End.ContainsNaN() || Rotation.ContainsNaN() ||
		(Shape && !Shape->IsValid()) || !BuildQuery(Context, Query, Objects) || Context.RemainingQueries < 2)
	{
		return false;
	}
	Context.RemainingQueries -= 2;
	TArray<FHitResult> Candidates;
	if (Shape)
	{
		Context.World->SweepMultiByObjectType(Candidates, Start, End, Rotation, Objects, Shape->MakeCollisionShape(), Query);
	}
	else
	{
		Context.World->LineTraceMultiByObjectType(Candidates, Start, End, Objects, Query);
	}
	FCollisionQueryParams BlockingQuery = Query;
	for (const FHitResult& Hit : Candidates)
	{
		if (IsValid(Hit.GetActor()) && Context.IsTargetActor(Hit.GetActor()))
		{
			BlockingQuery.AddIgnoredActor(Hit.GetActor());
		}
	}
	if (Shape)
	{
		Context.World->SweepSingleByChannel(OutBlocker, Start, End, Rotation, Context.BlockingChannel,
			Shape->MakeCollisionShape(), BlockingQuery);
	}
	else
	{
		Context.World->LineTraceSingleByChannel(OutBlocker, Start, End, Context.BlockingChannel, BlockingQuery);
	}
	const float MaximumTime = OutBlocker.bBlockingHit ? OutBlocker.Time : 1.f;
	for (const FHitResult& Hit : Candidates)
	{
		if (Hit.Time <= MaximumTime + UE_KINDA_SMALL_NUMBER && IsValid(Hit.GetActor()) && Context.IsTargetActor(Hit.GetActor()))
		{
			OutHits.Add(Hit);
		}
	}
	OutHits.Sort([](const FHitResult& A, const FHitResult& B)
	{
		if (!FMath::IsNearlyEqual(A.Time, B.Time))
		{
			return A.Time < B.Time;
		}
		return GetTypeHash(A.GetActor()) < GetTypeHash(B.GetActor());
	});
	return true;
}

bool FAbilityHitQuery::HasLineOfSight(FAbilityHitQueryContext& Context, const FVector& Start, AActor* Target,
	const FVector& End, bool& bOutVisible)
{
	bOutVisible = false;
	TArray<FHitResult> Hits;
	FHitResult Blocker;
	if (!IsValid(Target) || !Trace(Context, Start, End, FQuat::Identity, nullptr, Hits, Blocker))
	{
		return false;
	}
	bOutVisible = !Blocker.bBlockingHit || Blocker.GetActor() == Target;
	return true;
}

bool FAbilityHitQuery::Gather(FAbilityHitQueryContext& Context, const FAbilityHitQueryDefinition& Definition,
	const FTransform& PreviousFrame, const FTransform& CurrentFrame, float PreviousAlpha, float CurrentAlpha,
	TArray<FHitResult>& OutHits)
{
	OutHits.Reset();
	if (PreviousFrame.ContainsNaN() || CurrentFrame.ContainsNaN() || !FMath::IsFinite(PreviousAlpha) ||
		!FMath::IsFinite(CurrentAlpha) || CurrentAlpha < PreviousAlpha)
	{
		return false;
	}
	if (Definition.Mode == EAbilityHitQueryMode::Overlap)
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(AbilityOverlapQuery), false);
		FCollisionObjectQueryParams Objects;
		if (!BuildQuery(Context, Query, Objects) || !Definition.Shape.IsValid())
		{
			return false;
		}
		--Context.RemainingQueries;
		const FVector Center = CurrentFrame.TransformPosition(Definition.LocalStart);
		const FQuat Rotation = CurrentFrame.GetRotation() * Definition.LocalRotation.Quaternion();
		TArray<FOverlapResult> Overlaps;
		Context.World->OverlapMultiByObjectType(Overlaps, Center, Rotation, Objects, Definition.Shape.MakeCollisionShape(), Query);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Target = Overlap.GetActor();
			if (IsValid(Target) && Context.IsTargetActor(Target))
			{
				OutHits.Add(FHitResult(Target, Overlap.Component.Get(), Target->GetActorLocation(),
					(Center - Target->GetActorLocation()).GetSafeNormal()));
			}
		}
		return true;
	}
	if (Definition.Mode == EAbilityHitQueryMode::Ray || Definition.Mode == EAbilityHitQueryMode::Sweep)
	{
		FHitResult Blocker;
		return Trace(Context, CurrentFrame.TransformPosition(Definition.LocalStart), CurrentFrame.TransformPosition(Definition.LocalEnd),
			CurrentFrame.GetRotation() * Definition.LocalRotation.Quaternion(),
			Definition.Mode == EAbilityHitQueryMode::Ray ? nullptr : &Definition.Shape, OutHits, Blocker);
	}
	if (Definition.Mode != EAbilityHitQueryMode::Trajectory || Definition.Trajectory.Num() < 2)
	{
		return false;
	}
	TArray<float> Cuts;
	Cuts.Add(FMath::Clamp(PreviousAlpha, 0.f, 1.f));
	for (const FAbilityHitTrajectoryKey& Key : Definition.Trajectory)
	{
		if (Key.Alpha > PreviousAlpha && Key.Alpha < CurrentAlpha)
		{
			Cuts.Add(Key.Alpha);
		}
	}
	Cuts.Add(FMath::Clamp(CurrentAlpha, 0.f, 1.f));
	const float Span = FMath::Max(UE_SMALL_NUMBER, CurrentAlpha - PreviousAlpha);
	for (int32 Cut = 1; Cut < Cuts.Num(); ++Cut)
	{
		const float Begin = Cuts[Cut - 1];
		const float End = Cuts[Cut];
		const FTransform FrameA = InterpolateFrame(PreviousFrame, CurrentFrame, (Begin - PreviousAlpha) / Span);
		const FTransform FrameB = InterpolateFrame(PreviousFrame, CurrentFrame, (End - PreviousAlpha) / Span);
		FVector A0, A1, B0, B1;
		Definition.EvaluateTrajectory(Begin, A0, A1);
		Definition.EvaluateTrajectory(End, B0, B1);
		const float Radius = Definition.Shape.Radius;
		const int32 BladeSamples = FMath::Max(1, FMath::CeilToInt(FMath::Max(FVector::Distance(A0, A1), FVector::Distance(B0, B1)) / Radius));
		const float Angle = FrameA.GetRotation().AngularDistance(FrameB.GetRotation());
		const int32 TimeSamples = FMath::Max(1, FMath::CeilToInt(Angle / FMath::DegreesToRadians(5.f)));
		if (BladeSamples > 32 || TimeSamples > 72 || 2 * (BladeSamples + 1) * TimeSamples > Context.RemainingQueries)
		{
			return false;
		}
		for (int32 Step = 0; Step < TimeSamples; ++Step)
		{
			const float T0 = static_cast<float>(Step) / TimeSamples;
			const float T1 = static_cast<float>(Step + 1) / TimeSamples;
			const FTransform RootA = InterpolateFrame(FrameA, FrameB, T0);
			const FTransform RootB = InterpolateFrame(FrameA, FrameB, T1);
			const float MaxLocalRadius = FMath::Max(FMath::Max(A0.Size(), A1.Size()), FMath::Max(B0.Size(), B1.Size()));
			FAbilityHitShape Shape = Definition.Shape;
			Shape.Radius += Radius * 0.5f + MaxLocalRadius * Angle / TimeSamples;
			for (int32 Sample = 0; Sample <= BladeSamples; ++Sample)
			{
				const float Along = static_cast<float>(Sample) / BladeSamples;
				const FVector LocalA = FMath::Lerp(FMath::Lerp(A0, B0, T0), FMath::Lerp(A1, B1, T0), Along);
				const FVector LocalB = FMath::Lerp(FMath::Lerp(A0, B0, T1), FMath::Lerp(A1, B1, T1), Along);
				TArray<FHitResult> Hits;
				FHitResult Blocker;
				if (!Trace(Context, RootA.TransformPosition(LocalA), RootB.TransformPosition(LocalB), FQuat::Identity, &Shape, Hits, Blocker))
				{
					return false;
				}
				OutHits.Append(Hits);
			}
		}
	}
	return true;
}
