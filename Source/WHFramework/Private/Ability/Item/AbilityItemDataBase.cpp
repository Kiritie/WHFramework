#include "Ability/Item/AbilityItemDataBase.h"

#include "Ability/AbilityModule.h"
#include "Ability/AbilityModuleStatics.h"
#include "Ability/Abilities/ItemAbilityBase.h"
#include "Ability/Actor/AbilityActorInterface.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
#include "Common/CommonModuleStatics.h"

UAbilityItemDataBase::UAbilityItemDataBase()
{
	Type = FName("Misc");

	Name = FText::GetEmpty();
	Detail = FText::GetEmpty();
	Rarity = EAbilityItemRarity::None;
	Icon = nullptr;
	MaxCount = 0;
	MaxLevel = 0;
	AbilityClass = nullptr;
	PickUpClass = nullptr;

	_Icon = nullptr;
}

void UAbilityItemDataBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	_Icon = Icon;
}

void UAbilityItemDataBase::OnReset_Implementation()
{
	Super::OnReset_Implementation();

	Icon = _Icon;
}

int32 UAbilityItemDataBase::ClampCount(int32 InCount) const
{
	return FMath::Clamp(InCount, 0, MaxCount != -1 ? MaxCount : InCount);
}

int32 UAbilityItemDataBase::ClampLevel(int32 InLevel) const
{
	return FMath::Clamp(InLevel, 0, MaxLevel != -1 ? MaxLevel : InLevel);
}

EAbilityItemType UAbilityItemDataBase::GetItemType() const
{
	return (EAbilityItemType)UCommonModuleStatics::GetEnumValueByAuthoredName(TEXT("/Script/WHFramework.EAbilityItemType"), Type.ToString());
}

FString UAbilityItemDataBase::GetItemAbilityInfo(int32 InLevel) const
{
	if(AbilityClass)
	{
		FString AbilityInfoStr;
		FAbilityInfo AbilityInfo = UAbilityModuleStatics::GetAbilityInfoByClass(AbilityClass, InLevel);
		for(auto& Iter1 : AbilityInfo.Effects)
		{
			for(auto& Iter2 : Iter1.Attributes)
			{
				FText AttributeName = UCommonModuleStatics::GetPropertyDisplayName(Iter2.Attribute.GetUProperty());
				if(Iter2.BaseAttribute.IsValid())
				{
					FText BaseAttributeName = UCommonModuleStatics::GetPropertyDisplayName(Iter2.BaseAttribute.GetUProperty());
					AbilityInfoStr += (Iter2.AttributeSource == EGameplayEffectAttributeCaptureSource::Source ? FText::Format(NSLOCTEXT("WH.AbilityItem", "SourceAttribute", "{0}: {1}%自身{2}\n"), AttributeName, FText::AsNumber((int32)(Iter2.Value * 100)), BaseAttributeName) : FText::Format(NSLOCTEXT("WH.AbilityItem", "TargetAttribute", "{0}: {1}%目标{2}\n"), AttributeName, FText::AsNumber((int32)(Iter2.Value * 100)), BaseAttributeName)).ToString();
				}
				else
				{
					AbilityInfoStr.Appendf(TEXT("%s: %s%s\n"), *AttributeName.ToString(), (Iter2.Attribute.GetName().EndsWith(TEXT("Rate")) ? *FString::Printf(TEXT("%s%%"), *UCommonModuleStatics::SanitizeFloat(Iter2.Value * 100, 2)) : *UCommonModuleStatics::SanitizeFloat(Iter2.Value, 2)),
						(Iter2.Attribute.GetName().EndsWith(TEXT("Time")) || Iter2.Attribute.GetName().EndsWith(TEXT("Interrupt")) ? TEXT("s") : TEXT("")));
				}
			}
			if(Iter1.Period > 0.f)
			{
				AbilityInfoStr += FText::Format(NSLOCTEXT("WH.AbilityItem", "Period", "触发周期: {0}s\n"), FText::FromString(UCommonModuleStatics::SanitizeFloat(Iter1.Period, 2))).ToString();
			}
			if(Iter1.Duration > 0.f)
			{
				AbilityInfoStr += FText::Format(NSLOCTEXT("WH.AbilityItem", "Duration", "持续时间: {0}s\n"), FText::FromString(UCommonModuleStatics::SanitizeFloat(Iter1.Duration, 2))).ToString();
			}
		}
		if(AbilityInfo.CooldownDuration > 0.f)
		{
			AbilityInfoStr += FText::Format(NSLOCTEXT("WH.AbilityItem", "Cooldown", "冷却时间: {0}s\n"), FText::FromString(UCommonModuleStatics::SanitizeFloat(AbilityInfo.CooldownDuration, 2))).ToString();
		}
		if(FMath::Abs(AbilityInfo.CostValue) > 0.f)
		{
			FText AttributeName = UCommonModuleStatics::GetPropertyDisplayName(AbilityInfo.CostAttribute.GetUProperty());
			AbilityInfoStr += FText::Format(NSLOCTEXT("WH.AbilityItem", "Cost", "消耗: {0}{1}\n"), FText::FromString(UCommonModuleStatics::SanitizeFloat(FMath::Abs(AbilityInfo.CostValue), 2)), AttributeName).ToString();
		}
		
		AbilityInfoStr.RemoveFromEnd(TEXT("\n"));
		
		return AbilityInfoStr;
	}
	return TEXT("");
}

FString UAbilityItemDataBase::GetItemErrorInfo(FAbilityItem InItem) const
{
	FString ErrorInfo;
	if(UAbilityInventorySlotBase* InventorySlot = InItem.GetPayload<UAbilityInventorySlotBase>())
	{
		if(IAbilityActorInterface* AbilityActor = InventorySlot->GetInventory()->GetOwnerAgent<IAbilityActorInterface>())
		{
			if(AbilityActor->GetLevelA() < InItem.Level)
			{
				ErrorInfo = FText::Format(NSLOCTEXT("WH.AbilityItem", "RequiredLevel", "角色未达到[{0}]级"), FText::AsNumber(InItem.Level)).ToString();
			}
		}
	}
	return ErrorInfo;
}
