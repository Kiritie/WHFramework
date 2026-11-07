#include "Ability/AbilityEffectTypes.h"
#include "Abilities/GameplayAbilityTargetTypes.h"

bool FGameplayEffectContextBase::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bool bBaseSuccess = true;
	bool bTargetSuccess = true;
	const bool bBaseMapped = Super::NetSerialize(Ar, Map, bBaseSuccess);
	if (Ar.IsLoading())
	{
		AddSourceObject(GetSourceObject());
	}
	const bool bTargetMapped = TargetData.NetSerialize(Ar, Map, bTargetSuccess);
	Ar << SourceId;
	Ar << EffectInstanceId;
	Ar << GrantSourceId;
	Ar << ExternalPersistenceDomain;
	bOutSuccess = bBaseSuccess && bTargetSuccess && !Ar.IsError();
	return bBaseMapped && bTargetMapped;
}

bool FGameplayEffectContainerSpec::HasValidTargets() const
{
	return TargetData.Num() > 0;
}

bool FGameplayEffectContainerSpec::HasValidEffects() const
{
	return TargetGameplayEffectSpecs.Num() > 0;
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
