#if WITH_DEV_AUTOMATION_TESTS

#include "Ability/Combat/AbilityHitQuery.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

namespace
{
	struct FAbilityQueryTestWorld
	{
		UWorld* World = nullptr;

		FAbilityQueryTestWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues().CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).AllowAudioPlayback(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}

		~FAbilityQueryTestWorld()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		AActor* Box(const FVector& Location, const FVector& Extent, ECollisionChannel Channel, bool bTarget)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Component = NewObject<UBoxComponent>(Actor);
			Actor->AddInstanceComponent(Component);
			Actor->SetRootComponent(Component);
			Component->SetBoxExtent(Extent);
			Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Component->SetCollisionObjectType(Channel);
			Component->SetCollisionResponseToAllChannels(ECR_Block);
			Component->RegisterComponent();
			Component->SetWorldLocation(Location);
			if (bTarget)
			{
				Actor->Tags.Add(TEXT("QueryTestTarget"));
			}
			return Actor;
		}

		FAbilityHitQueryContext Context() const
		{
			FAbilityHitQueryContext Context;
			Context.World = World;
			Context.TargetObjectChannels = {ECC_Pawn, ECC_GameTraceChannel1, ECC_GameTraceChannel13};
			Context.BlockingChannel = ECC_Visibility;
			Context.RemainingQueries = 2048;
			Context.IsTargetActor = [](AActor* Actor)
			{
				return Actor && Actor->ActorHasTag(TEXT("QueryTestTarget"));
			};
			return Context;
		}
	};

	bool ContainsActor(const TArray<FHitResult>& Hits, AActor* Actor)
	{
		return Hits.ContainsByPredicate([Actor](const FHitResult& Hit)
		{
			return Hit.GetActor() == Actor;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityMixedChannelQueryTest, "WHFramework.Ability.Combat.MixedChannelsAndWall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAbilityMixedChannelQueryTest::RunTest(const FString& Parameters)
{
	FAbilityQueryTestWorld Fixture;
	AActor* Character = Fixture.Box(FVector(100.f, 0.f, 0.f), FVector(10.f), ECC_GameTraceChannel1, true);
	AActor* Pawn = Fixture.Box(FVector(200.f, 0.f, 0.f), FVector(10.f), ECC_Pawn, true);
	AActor* Vitality = Fixture.Box(FVector(300.f, 0.f, 0.f), FVector(10.f), ECC_GameTraceChannel13, true);
	AActor* Wall = Fixture.Box(FVector(500.f, 0.f, 0.f), FVector(10.f, 100.f, 100.f), ECC_WorldStatic, false);
	AActor* BehindWall = Fixture.Box(FVector(700.f, 0.f, 0.f), FVector(10.f), ECC_GameTraceChannel1, true);
	FAbilityHitQueryContext Context = Fixture.Context();
	TArray<FHitResult> Hits;
	FHitResult Blocker;
	TestTrue(TEXT("射线查询完成"), FAbilityHitQuery::Trace(Context, FVector::ZeroVector, FVector(1000.f, 0.f, 0.f),
		FQuat::Identity, nullptr, Hits, Blocker));
	TestTrue(TEXT("自定义角色通道命中"), ContainsActor(Hits, Character));
	TestTrue(TEXT("Pawn通道命中"), ContainsActor(Hits, Pawn));
	TestTrue(TEXT("生命体通道命中"), ContainsActor(Hits, Vitality));
	TestFalse(TEXT("墙后目标被裁剪"), ContainsActor(Hits, BehindWall));
	TestEqual(TEXT("阻挡物是世界墙体"), Blocker.GetActor(), Wall);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilitySweptProjectileQueryTest, "WHFramework.Ability.Combat.SweepKeepsBlockingTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAbilitySweptProjectileQueryTest::RunTest(const FString& Parameters)
{
	FAbilityQueryTestWorld Fixture;
	AActor* Target = Fixture.Box(FVector(250.f, 0.f, 0.f), FVector(15.f), ECC_GameTraceChannel1, true);
	AActor* Wall = Fixture.Box(FVector(450.f, 0.f, 0.f), FVector(10.f, 100.f, 100.f), ECC_WorldStatic, false);
	FAbilityHitQueryContext Context = Fixture.Context();
	FAbilityHitShape Shape;
	Shape.Type = EAbilityHitShape::Capsule;
	Shape.Radius = 8.f;
	Shape.HalfHeight = 24.f;
	TArray<FHitResult> Hits;
	FHitResult Blocker;
	TestTrue(TEXT("快速弹体跨越整段时完成形状扫掠"), FAbilityHitQuery::Trace(Context, FVector::ZeroVector,
		FVector(1000.f, 0.f, 0.f), FQuat::Identity, &Shape, Hits, Blocker));
	TestTrue(TEXT("对Visibility阻挡的角色仍先进入结算候选"), ContainsActor(Hits, Target));
	TestEqual(TEXT("之后的墙体仍保留终止点"), Blocker.GetActor(), Wall);
	bool bVisible = true;
	Context = Fixture.Context();
	AActor* Hidden = Fixture.Box(FVector(600.f, 0.f, 0.f), FVector(15.f), ECC_Pawn, true);
	TestTrue(TEXT("视线查询完成"), FAbilityHitQuery::HasLineOfSight(Context, FVector::ZeroVector, Hidden,
		Hidden->GetActorLocation(), bVisible));
	TestFalse(TEXT("范围攻击不得越过墙体"), bVisible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityTrajectoryCatchupTest, "WHFramework.Ability.Combat.TrajectoryCrossesKeyframes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAbilityTrajectoryCatchupTest::RunTest(const FString& Parameters)
{
	FAbilityQueryTestWorld Fixture;
	AActor* Target = Fixture.Box(FVector(100.f, 0.f, 0.f), FVector(5.f), ECC_GameTraceChannel13, true);
	FAbilityHitQueryDefinition Query;
	Query.Mode = EAbilityHitQueryMode::Trajectory;
	Query.Shape.Radius = 5.f;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FAbilityHitTrajectoryKey Key;
		Key.Alpha = Index * 0.5f;
		Key.LocalStart = FVector(100.f, Index == 1 ? 100.f : -100.f, 0.f);
		Key.LocalEnd = Key.LocalStart;
		Query.Trajectory.Add(Key);
	}
	FString Error;
	TestTrue(TEXT("显式轨迹配置有效"), Query.Validate(300.f, Error));
	FAbilityHitQueryContext Context = Fixture.Context();
	TArray<FHitResult> Hits;
	TestTrue(TEXT("一个低帧率tick跨越全部关键帧"), FAbilityHitQuery::Gather(Context, Query,
		FTransform::Identity, FTransform::Identity, 0.f, 1.f, Hits));
	TestTrue(TEXT("端点都未接触但中途经过目标仍命中"), ContainsActor(Hits, Target));
	Context = Fixture.Context();
	Context.RemainingQueries = 1;
	TestFalse(TEXT("查询预算不足明确失败，不能当作合法空命中"), FAbilityHitQuery::Gather(Context, Query,
		FTransform::Identity, FTransform::Identity, 0.f, 1.f, Hits));
	return true;
}

#endif
