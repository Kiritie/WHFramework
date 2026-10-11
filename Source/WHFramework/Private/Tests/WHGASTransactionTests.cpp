#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/WHGASTestTypes.h"
#include "Ability/Attributes/VitalityAttributeSetBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"

namespace WHGASTests
{
	struct FWorldFixture
	{
		TStrongObjectPtr<UWorld> World;
		AActor* Owner = nullptr;
		UAbilitySystemComponentBase* AbilitySystem = nullptr;

		bool Initialize(FAutomationTestBase& Test)
		{
			World.Reset(UWorld::CreateWorld(EWorldType::Game, false));
			if (!Test.TestNotNull(TEXT("Isolated world"), World.Get()) || !Test.TestNotNull(TEXT("Engine"), GEngine))
			{
				return false;
			}
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World.Get());
			Owner = World->SpawnActor<AActor>();
			if (!Test.TestNotNull(TEXT("Authority owner"), Owner))
			{
				return false;
			}
			AbilitySystem = NewObject<UAbilitySystemComponentBase>(Owner);
			Owner->AddInstanceComponent(AbilitySystem);
			AbilitySystem->RegisterComponent();
			UVitalityAttributeSetBase* Attributes = NewObject<UVitalityAttributeSetBase>(Owner);
			AbilitySystem->AddAttributeSetSubobject(Attributes);
			AbilitySystem->SetAbilityEntityId(FGuid(101, 102, 103, 104));
			if (!Test.TestTrue(TEXT("ActorInfo is bound"), AbilitySystem->BindOwnerAvatar(Owner, Owner)))
			{
				return false;
			}
			AbilitySystem->SetNumericAttributeBase(UVitalityAttributeSetBase::GetHealthRegenSpeedAttribute(), 2.f);
			return true;
		}

		float Regeneration() const
		{
			return AbilitySystem->GetNumericAttribute(UVitalityAttributeSetBase::GetHealthRegenSpeedAttribute());
		}

		~FWorldFixture()
		{
			if (World.IsValid())
			{
				World->DestroyWorld(false);
				if (GEngine)
				{
					GEngine->DestroyWorldContext(World.Get());
				}
			}
		}
	};

	FAbilityGrantSourceDefinition MakeSource(FGuid SourceId, FName DefinitionName, int32 Level)
	{
		FAbilityGrantSourceDefinition Source;
		Source.SourceId = SourceId;
		Source.Level = Level;
		FAbilityGrantDefinition Ability;
		Ability.DefinitionId = FPrimaryAssetId(TEXT("TestAbility"), DefinitionName);
		Ability.AbilityClass = UWHGASTestAbility::StaticClass();
		Ability.SourceObject = UWHGASTestAbility::StaticClass()->GetDefaultObject();
		Ability.Level = Level;
		Source.Abilities.Add(Ability);
		Source.Effects.Add(UWHGASTestAttributeEffect::StaticClass());
		return Source;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWHGASGrantSetAtomicityTest, "WHFramework.GAS.GrantSetFailureKeepsPreviousRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWHGASGrantSetAtomicityTest::RunTest(const FString& Parameters)
{
	WHGASTests::FWorldFixture Fixture;
	if (!Fixture.Initialize(*this))
	{
		return false;
	}
	const FGuid OldSourceId(1, 2, 3, 4);
	const FGuid NewSourceId(5, 6, 7, 8);
	const FGuid InvalidSourceId(9, 10, 11, 12);
	FAbilityGrantSourceDefinition Original = WHGASTests::MakeSource(OldSourceId, TEXT("Original"), 1);
	TestTrue(TEXT("Initial complete grant is accepted"), Fixture.AbilitySystem->ReplaceGrantSourceSet({}, {Original}));
	FGameplayAbilitySpecHandle OriginalHandle;
	if (!TestTrue(TEXT("Original skill exists"), Fixture.AbilitySystem->FindGrantedAbility(Original.Abilities[0].DefinitionId, AbilityActionTags::Request, OriginalHandle)))
	{
		return false;
	}
	TestEqual(TEXT("Initial source applies its modifier"), Fixture.Regeneration(), 5.f);
	FAbilityGrantSourceDefinition Replacement = Original;
	Replacement.Level = 2;
	Replacement.Abilities[0].Level = 2;
	FAbilityGrantSourceDefinition Additional = WHGASTests::MakeSource(NewSourceId, TEXT("Additional"), 1);
	FAbilityGrantSourceDefinition Invalid = WHGASTests::MakeSource(InvalidSourceId, TEXT("Invalid"), 1);
	Invalid.Effects = {UWHGASTestInstantEffect::StaticClass()};
	TSet<FGuid> Owned;
	Owned.Add(OldSourceId);
	TestFalse(TEXT("An invalid final GE rejects the entire source set"), Fixture.AbilitySystem->ReplaceGrantSourceSet(Owned, {Replacement, Additional, Invalid}));
	FGameplayAbilitySpecHandle CurrentHandle;
	TestTrue(TEXT("Old definition is still granted"), Fixture.AbilitySystem->FindGrantedAbility(Original.Abilities[0].DefinitionId, AbilityActionTags::Request, CurrentHandle));
	TestTrue(TEXT("Failure preserves the original runtime spec handle"), CurrentHandle == OriginalHandle);
	const FGameplayAbilitySpec* CurrentSpec = Fixture.AbilitySystem->FindAbilitySpecFromHandle(OriginalHandle);
	TestTrue(TEXT("Failure preserves the old level"), CurrentSpec && CurrentSpec->Level == 1);
	FGameplayAbilitySpecHandle Unexpected;
	TestFalse(TEXT("No preceding valid candidate leaked into the runtime"), Fixture.AbilitySystem->FindGrantedAbility(Additional.Abilities[0].DefinitionId, AbilityActionTags::Request, Unexpected));
	TestEqual(TEXT("No candidate attribute modifiers leaked"), Fixture.Regeneration(), 5.f);
	TestEqual(TEXT("Only the original spec remains"), Fixture.AbilitySystem->GetGrantedEntries().Num(), 1);
	TestEqual(TEXT("Only the original source effect remains"), Fixture.AbilitySystem->GetActiveEffects(FGameplayEffectQuery()).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWHGASEffectRestoreIdempotenceTest, "WHFramework.GAS.EffectRestoreIdentityAndFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWHGASEffectRestoreIdempotenceTest::RunTest(const FString& Parameters)
{
	WHGASTests::FWorldFixture Fixture;
	if (!Fixture.Initialize(*this))
	{
		return false;
	}
	FGameplayEffectContextHandle Context = Fixture.AbilitySystem->MakeEffectContext();
	Context.AddSourceObject(UWHGASTestAttributeEffect::StaticClass()->GetDefaultObject());
	FGameplayEffectSpecHandle Spec = Fixture.AbilitySystem->MakeOutgoingSpec(UWHGASTestAttributeEffect::StaticClass(), 1.f, Context);
	if (!TestTrue(TEXT("Persistent spec is valid"), Spec.IsValid()))
	{
		return false;
	}
	const FActiveGameplayEffectHandle Original = Fixture.AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!TestTrue(TEXT("Persistent effect applied"), Original.IsValid()))
	{
		return false;
	}
	TArray<FAbilityEffectSaveData> Snapshot;
	if (!TestTrue(TEXT("Persistent effect captures"), Fixture.AbilitySystem->CapturePersistentEffects(Snapshot))
		|| !TestEqual(TEXT("One effect is captured"), Snapshot.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("Same active identity restores idempotently"), Fixture.AbilitySystem->RestorePersistentEffects(Snapshot));
	TestEqual(TEXT("Idempotent restore does not double the modifier"), Fixture.Regeneration(), 5.f);
	TestEqual(TEXT("Idempotent restore does not create a second handle"), Fixture.AbilitySystem->GetActiveEffects(FGameplayEffectQuery()).Num(), 1);
	TArray<FAbilityEffectSaveData> DuplicateIds = Snapshot;
	DuplicateIds.Add(Snapshot[0]);
	TestFalse(TEXT("A snapshot cannot contain a duplicate effect identity"), Fixture.AbilitySystem->RestorePersistentEffects(DuplicateIds));
	TestTrue(TEXT("Duplicate identity rejection retains the existing handle"), Fixture.AbilitySystem->GetActiveGameplayEffect(Original) != nullptr);
	TestEqual(TEXT("Duplicate identity rejection retains the old value"), Fixture.Regeneration(), 5.f);
	TArray<FAbilityEffectSaveData> InvalidBatch = Snapshot;
	FAbilityEffectSaveData Invalid = Snapshot[0];
	Invalid.EffectInstanceId = FGuid(21, 22, 23, 24);
	Invalid.EffectClass = FSoftClassPath(UWHGASTestInstantEffect::StaticClass());
	InvalidBatch.Add(Invalid);
	TestFalse(TEXT("An unsupported final effect rejects the complete restore"), Fixture.AbilitySystem->RestorePersistentEffects(InvalidBatch));
	TestEqual(TEXT("Rejected restore does not mutate the old value"), Fixture.Regeneration(), 5.f);
	Fixture.AbilitySystem->ClearPersistentEffects();
	TestEqual(TEXT("Explicit clear removes the prior modifier"), Fixture.Regeneration(), 2.f);
	TestTrue(TEXT("A cleared runtime restores from the same snapshot"), Fixture.AbilitySystem->RestorePersistentEffects(Snapshot));
	TestTrue(TEXT("Repeated restore remains idempotent after a real load"), Fixture.AbilitySystem->RestorePersistentEffects(Snapshot));
	TestEqual(TEXT("Reload applies the saved effect exactly once"), Fixture.Regeneration(), 5.f);
	TestEqual(TEXT("Reload keeps exactly one effect"), Fixture.AbilitySystem->GetActiveEffects(FGameplayEffectQuery()).Num(), 1);
	return true;
}

#endif
