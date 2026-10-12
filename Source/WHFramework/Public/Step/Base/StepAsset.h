#pragma once

#include "CoreMinimal.h"
#include "Asset/Base/AssetBase.h"
#include "Step/StepModuleTypes.h"
#include "StepAsset.generated.h"

class UStepBase;

UCLASS(BlueprintType, Meta = (LoadBehavior = "LazyOnDemand"))
class WHFRAMEWORK_API UStepAsset : public UAssetBase
{
	GENERATED_BODY()

public:
	UStepAsset(const FObjectInitializer& ObjectInitializer);

	virtual void Initialize() override;

	UPROPERTY(EditAnywhere)
	bool bAutoStartFirst = false;

	UPROPERTY(Instanced)
	TArray<UStepBase*> Steps;

	UPROPERTY()
	FString StartGUID;

	UPROPERTY()
	FVector2D StartPosition = FVector2D::ZeroVector;

	UPROPERTY()
	TArray<FStepGraphLink> Links;

	UPROPERTY()
	FStepCompiledGraph CompiledGraph;

	UPROPERTY()
	bool bCompiled = false;

	bool BuildCompiledGraph(FStepCompiledGraph& OutGraph, TArray<FText>& OutErrors) const;
	bool Compile(TArray<FText>& OutErrors);
	UStepBase* FindDefinition(const FString& InGUID) const;
	bool CanAddStep(TSubclassOf<UStepBase> InClass) const;

#if WITH_EDITOR
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
