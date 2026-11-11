#pragma once

#include "Setting/SettingModuleTypes.h"
#include "Setting/SettingDefinitionTypes.h"

#include "SettingProviderBase.generated.h"

class UWHLocalPlayer;

UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class WHFRAMEWORK_API USettingProviderBase : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;

	virtual void CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const;

	virtual bool CanHandle(const FSettingDefinition& InDefinition) const;

	virtual void BeginEdit(const TArray<FSettingDefinition>& InDefinitions);

	virtual FParameter GetAppliedValue(const FSettingDefinition& InDefinition) const;

	virtual FParameter GetPendingValue(const FSettingDefinition& InDefinition) const;

	virtual FParameter GetDefaultValue(const FSettingDefinition& InDefinition) const;

	virtual FSettingValidationResult Validate(const FSettingDefinition& InDefinition, const FParameter& InValue) const;

	virtual bool SetPendingValue(const FSettingDefinition& InDefinition, const FParameter& InValue);

	virtual bool Preview(const FSettingDefinition& InDefinition, const FParameter& InValue);

	virtual bool Apply(const FSettingDefinition& InDefinition, const FParameter& InValue);

	virtual bool Rollback(const FSettingDefinition& InDefinition);

	// 世界已不可用时仅恢复由原 LocalPlayer 持有的状态，不访问世界服务。
	virtual bool RollbackForHostTermination(const FSettingDefinition& InDefinition, UWHLocalPlayer* InPlayer);
	virtual bool CanRollbackForHostTermination(const FSettingDefinition& InDefinition, const UWHLocalPlayer* InPlayer) const;

	virtual void Commit();

	virtual void EndEdit();
};
