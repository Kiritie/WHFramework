#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/WHGASTestTypes.h"
#include "Ability/Abilities/ItemAbilityBase.h"
#include "Ability/Effects/Calculation/UEffectCalculation_Level.h"
#include "Ability/Item/AbilityItemDataBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityItemAttributeValuesTest, "WHFramework.Ability.Item.EquipmentAttributeValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAbilityItemAttributeValuesTest::RunTest(const FString& Parameters)
{
    UItemAbilityBase* Ability = GetMutableDefault<UItemAbilityBase>();
    UWHGASTestAttributeEffect* Effect = GetMutableDefault<UWHGASTestAttributeEffect>();
    TGuardValue<TArray<TSubclassOf<UEffectBase>>> AbilityGuard(Ability->EffectClasses, {});
    TGuardValue<TArray<FGameplayModifierInfo>> ModifierGuard(Effect->Modifiers, Effect->Modifiers);
    FInventoryItemDefinition Definition;
    Definition.UsePolicy = EInventoryUsePolicy::Equip;
    Definition.AbilityClass = FSoftClassPath(UItemAbilityBase::StaticClass());
    Definition.PassiveEffects.Add(FSoftClassPath(UWHGASTestAttributeEffect::StaticClass()));
    Ability->EffectClasses.Add(UWHGASTestAttributeEffect::StaticClass());
    const FGameplayModifierInfo ConstantModifier = Effect->Modifiers[0];
    Effect->Modifiers.Add(ConstantModifier);
    TArray<FAbilityItemAttributeValue> Values;
    TestTrue(TEXT("Ability and passive effects can be evaluated"), UAbilityItemDataBase::GetEquipmentAttributeValues(Definition, 1, Values));
    if (TestEqual(TEXT("Same attribute and channel combine"), Values.Num(), 1))
    {
        TestEqual(TEXT("An effect shared by ability and passive lists is applied once"), Values[0].Value, 6.f);
    }

    FCustomCalculationBasedFloat LevelMagnitude;
    LevelMagnitude.CalculationClassMagnitude = UUEffectCalculation_Level::StaticClass();
    Effect->Modifiers.SetNum(1);
    Effect->Modifiers[0].ModifierMagnitude = FGameplayEffectModifierMagnitude(LevelMagnitude);
    TestTrue(TEXT("Level magnitude uses the effect specification"), UAbilityItemDataBase::GetEquipmentAttributeValues(Definition, 7, Values));
    if (TestEqual(TEXT("Level effect produces one value"), Values.Num(), 1))
    {
        TestEqual(TEXT("Evaluated level is not a default zero"), Values[0].Value, 7.f);
    }
    TestTrue(TEXT("Level zero follows equipment minimum level"), UAbilityItemDataBase::GetEquipmentAttributeValues(Definition, 0, Values));
    if (Values.Num() == 1)
    {
        TestEqual(TEXT("Minimum equipment source level is one"), Values[0].Value, 1.f);
    }

    Effect->Modifiers[0].SourceTags.RequireTags.AddTag(AbilityActionTags::Request);
    const FGameplayModifierInfo ConditionalModifier = Effect->Modifiers[0];
    Effect->Modifiers.Add(ConditionalModifier);
    TestTrue(TEXT("Conditional magnitudes remain readable"), UAbilityItemDataBase::GetEquipmentAttributeValues(Definition, 2, Values));
    if (TestEqual(TEXT("Conditional modifiers are not combined"), Values.Num(), 2))
    {
        TestTrue(TEXT("The view retains source conditions"), Values[0].bConditional && Values[1].bConditional);
    }

    Definition.PassiveEffects.Add(FSoftClassPath(UWHGASTestInstantEffect::StaticClass()));
    TestFalse(TEXT("An unsupported effect rejects the complete profile"), UAbilityItemDataBase::GetEquipmentAttributeValues(Definition, 1, Values));
    TestTrue(TEXT("Failure exposes no partial values or fake zero"), Values.IsEmpty());
    Definition.PassiveEffects.Reset();
    Definition.AbilityClass.Reset();
    TestTrue(TEXT("A valid empty equipment profile is known"), UAbilityItemDataBase::GetEquipmentAttributeValues(Definition, 1, Values));
    TestTrue(TEXT("Known empty equipment has no modifiers"), Values.IsEmpty());
    Definition.UsePolicy = EInventoryUsePolicy::Consumable;
    TestFalse(TEXT("A consumable is not interpreted as equipment"), UAbilityItemDataBase::GetEquipmentAttributeValues(Definition, 1, Values));
    return true;
}

#endif
