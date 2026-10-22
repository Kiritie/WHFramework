#include "Ability/AbilityActionTypes.h"

#include "Engine/PackageMapClient.h"
#include "GameFramework/Actor.h"

namespace AbilityActionTags
{
	UE_DEFINE_GAMEPLAY_TAG(ItemUse, "Ability.Action.ItemUse");
	UE_DEFINE_GAMEPLAY_TAG(Request, "Ability.Action.Request");
	UE_DEFINE_GAMEPLAY_TAG(StaticControl, "State.Vitality.Static");
	UE_DEFINE_GAMEPLAY_TAG(InvalidRequest, "Ability.Action.Reject.InvalidRequest");
	UE_DEFINE_GAMEPLAY_TAG(NotReady, "Ability.Action.Reject.NotReady");
	UE_DEFINE_GAMEPLAY_TAG(NotGranted, "Ability.Action.Reject.NotGranted");
	UE_DEFINE_GAMEPLAY_TAG(AlreadyActive, "Ability.Action.Reject.AlreadyActive");
	UE_DEFINE_GAMEPLAY_TAG(ActivationFailed, "Ability.Action.Reject.ActivationFailed");
	UE_DEFINE_GAMEPLAY_TAG(InvalidTarget, "Ability.Action.Reject.InvalidTarget");
	UE_DEFINE_GAMEPLAY_TAG(OutOfRange, "Ability.Action.Reject.OutOfRange");
	UE_DEFINE_GAMEPLAY_TAG(Blocked, "Ability.Action.Reject.Blocked");
	UE_DEFINE_GAMEPLAY_TAG(Cancelled, "Ability.Action.Cancelled");
	UE_DEFINE_GAMEPLAY_TAG(AvatarRemoved, "Ability.Action.Cancelled.AvatarRemoved");
	UE_DEFINE_GAMEPLAY_TAG(SourceRemoved, "Ability.Action.Cancelled.SourceRemoved");
	UE_DEFINE_GAMEPLAY_TAG(TargetDataTimeout, "Ability.Action.Failed.TargetDataTimeout");
}

UScriptStruct* FAbilityActionTargetData::GetScriptStruct() const
{
	return StaticStruct();
}

bool FAbilityActionTargetData::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = Map != nullptr;
	if (!Map)
	{
		return false;
	}
	Ar << ActivationId;
	FName AssetType = Request.DefinitionId.PrimaryAssetType.GetName();
	FName AssetName = Request.DefinitionId.PrimaryAssetName;
	Ar << AssetType;
	Ar << AssetName;
	bool bTagSuccess = true;
	Request.ActionTag.NetSerialize(Ar, Map, bTagSuccess);
	bOutSuccess &= bTagSuccess;
	Request.Variant.NetSerialize(Ar, Map, bTagSuccess);
	bOutSuccess &= bTagSuccess;
	UObject* Target = Request.TargetActor.Get();
	const bool bTargetMapped = Map->SerializeObject(Ar, AActor::StaticClass(), Target);
	FVector Location = Request.TargetLocation;
	FVector_NetQuantizeNormal Direction(Request.AimDirection.GetSafeNormal());
	FVector_NetQuantizeNormal Movement(Request.MovementDirection.GetSafeNormal());
	bool bVectorSuccess = true;
	Ar << Location;
	Direction.NetSerialize(Ar, Map, bVectorSuccess);
	bOutSuccess &= bVectorSuccess;
	Movement.NetSerialize(Ar, Map, bVectorSuccess);
	bOutSuccess &= bVectorSuccess;
	Ar << Request.WeaponPart;
	Ar << Request.ComboIndex;
	if (Ar.IsLoading())
	{
		Request.DefinitionId = FPrimaryAssetId(FPrimaryAssetType(AssetType), AssetName);
		Request.TargetActor = Cast<AActor>(Target);
		Request.TargetLocation = Location;
		Request.AimDirection = Direction;
		Request.MovementDirection = Movement;
		Request.AbilityHandle = FGameplayAbilitySpecHandle();
	}
	bOutSuccess &= !Ar.IsError() && ActivationId.IsValid() && Request.DefinitionId.IsValid() && Request.ActionTag.IsValid();
	bOutSuccess &= !Request.TargetLocation.ContainsNaN() && !Request.AimDirection.ContainsNaN() && !Request.MovementDirection.ContainsNaN();
	bOutSuccess &= Request.WeaponPart >= 0 && Request.WeaponPart <= 255 && Request.ComboIndex >= INDEX_NONE && Request.ComboIndex <= 255;
	return bTargetMapped;
}

bool FAbilityActionTargetData::Read(const FGameplayEventData* EventData, FGuid& OutActivationId, FAbilityActionRequest& OutRequest)
{
	if (!EventData || EventData->EventTag != AbilityActionTags::Request || EventData->TargetData.Num() != 1)
	{
		return false;
	}
	const FGameplayAbilityTargetData* Data = EventData->TargetData.Get(0);
	if (!Data || Data->GetScriptStruct() != StaticStruct())
	{
		return false;
	}
	const FAbilityActionTargetData* ActionData = static_cast<const FAbilityActionTargetData*>(Data);
	if (!ActionData->ActivationId.IsValid())
	{
		return false;
	}
	OutActivationId = ActionData->ActivationId;
	OutRequest = ActionData->Request;
	return true;
}
