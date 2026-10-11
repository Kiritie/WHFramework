#include "Ability/AbilityModuleTypes.h"

#include "Ability/Actor/AbilityActorDataBase.h"
#include "Asset/AssetModuleStatics.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Vitality/AbilityVitalityInterface.h"
#include "Common/CommonModuleStatics.h"

FPrimaryAssetId PAID_EXP = FPrimaryAssetId(TEXT("Misc:DA_Exp"));

bool FGameplayEffectContextBase::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	return Super::NetSerialize(Ar, Map, bOutSuccess) && TargetData.NetSerialize(Ar, Map, bOutSuccess);
}

void UTargetType::GetTargets_Implementation(AActor* OwningActor, AActor* TargetingActor, FGameplayEventData EventData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const
{
	
}

void UTargetType_UseOwner::GetTargets_Implementation(AActor* OwningActor, AActor* TargetingActor, FGameplayEventData EventData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const
{
	OutActors.Add(OwningActor);
}

void UTargetType_UseEventData::GetTargets_Implementation(AActor* OwningActor, AActor* TargetingActor, FGameplayEventData EventData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const
{
	const FHitResult* FoundHitResult = EventData.ContextHandle.GetHitResult();
	if (FoundHitResult)
	{
		OutHitResults.Add(*FoundHitResult);
	}
	else if (EventData.Target.Get())
	{
		OutActors.Add(const_cast<AActor*>(EventData.Target.Get()));
	}
}

void UDamageHandle::HandleDamage(AActor* SourceActor, AActor* TargetActor, float DamageValue, const FGameplayAttribute& DamageAttribute, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags)
{
	IAbilityVitalityInterface* TargetVitality = Cast<IAbilityVitalityInterface>(TargetActor);

	if (DamageValue > 0.f)
	{
		if (TargetVitality && !TargetVitality->IsDead())
		{
			TargetVitality->HandleDamage(DamageAttribute, DamageValue, false, false, HitResult, SourceTags, SourceActor);
		}
	}
}

void URecoveryHandle::HandleRecovery(AActor* SourceActor, AActor* TargetActor, float RecoveryValue, const FGameplayAttribute& RecoveryAttribute, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags)
{
	IAbilityVitalityInterface* TargetVitality = Cast<IAbilityVitalityInterface>(TargetActor);

	if (RecoveryValue > 0.f)
	{
		if (TargetVitality && !TargetVitality->IsDead())
		{
			TargetVitality->HandleRecovery(RecoveryAttribute, RecoveryValue, HitResult, SourceTags, SourceActor);
		}
	}
}

void UInterruptHandle::HandleInterrupt(AActor* SourceActor, AActor* TargetActor, float InterruptDuration, const FGameplayAttribute& InterruptAttribute, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags)
{
	IAbilityVitalityInterface* TargetVitality = Cast<IAbilityVitalityInterface>(TargetActor);

	if (InterruptDuration > 0.f)
	{
		if (TargetVitality && !TargetVitality->IsDead())
		{
			TargetVitality->HandleInterrupt(InterruptAttribute, InterruptDuration, HitResult, SourceTags, SourceActor);
		}
	}
}

bool FGameplayEffectContainerSpec::HasValidTargets() const
{
	return TargetGameplayEffectSpecs.Num() > 0;
}

bool FGameplayEffectContainerSpec::HasValidEffects() const
{
	return TargetData.Num() > 0;
}

void FGameplayEffectContainerSpec::AddTargets(const TArray<FHitResult>& HitResults, const TArray<AActor*>& TargetActors)
{
	for (const FHitResult& HitResult : HitResults)
	{
		// 创建单个射线目标数据对象
		FGameplayAbilityTargetData_SingleTargetHit* NewData = new FGameplayAbilityTargetData_SingleTargetHit(HitResult);
		// 将目标数据加入列表
		TargetData.Add(NewData);
	}

	if (TargetActors.Num() > 0)
	{
		// 创建目标Actor数组对象
		FGameplayAbilityTargetData_ActorArray* NewData = new FGameplayAbilityTargetData_ActorArray();
		NewData->TargetActorArray.Append(TargetActors);
		// 将目标数据加入列表
		TargetData.Add(NewData);
	}
}


UPrimaryAssetBase& FAbilityData::GetData() const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UPrimaryAssetBase>(ID);
}


UAbilityItemDataBase& FRaceItem::GetData() const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UAbilityItemDataBase>(ID);
}

void FActorSaveData::InitData(FRandomStream InRandomStream)
{
	const UAbilityActorDataBase& Definition = GetData();
	Level = Definition.ClampLevel(Level);
	if (Name.IsNone())
	{
		Name = FName(*Definition.Name.ToString());
	}
}

UAbilityActorDataBase& FActorSaveData::GetData() const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UAbilityActorDataBase>(AssetID);
}

