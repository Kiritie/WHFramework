#pragma once

#include "CoreMinimal.h"
#include "Asset/Base/AssetBase.h"
#include "Procedure/ProcedureModuleTypes.h"
#include "ProcedureAsset.generated.h"

class UProcedureBase;

UCLASS(BlueprintType, Meta = (LoadBehavior = "LazyOnDemand"))
class WHFRAMEWORK_API UProcedureAsset : public UAssetBase
{
    GENERATED_BODY()

public:
    UProcedureAsset(const FObjectInitializer& ObjectInitializer);

    virtual void Initialize() override;

    UPROPERTY(EditAnywhere)
    bool bAutoStart = true;

#if WITH_EDITORONLY_DATA
    UPROPERTY()
    FGuid StartId;

    UPROPERTY()
    FVector2D StartPosition = FVector2D::ZeroVector;

    UPROPERTY(Instanced)
    TArray<TObjectPtr<UProcedureBase>> Nodes;

    UPROPERTY()
    TArray<FProcedureLink> Links;
#endif

    UPROPERTY(VisibleAnywhere)
    bool bCompiled = false;

    UPROPERTY(VisibleAnywhere)
    FGuid EntryNodeId;

    UPROPERTY(VisibleAnywhere, Instanced)
    TArray<TObjectPtr<UProcedureBase>> CompiledNodes;

    UPROPERTY()
    TArray<FProcedureLink> CompiledLinks;

    UPROPERTY(VisibleAnywhere)
    FString CompileError;

    bool ValidateCompiled(FString& OutError) const;

#if WITH_EDITOR
    bool Recompile();
    bool BuildCompiled(TArray<UProcedureBase*>& OutNodes, TArray<FProcedureLink>& OutLinks, FGuid& OutEntry, FString& OutError) const;
    UProcedureBase* FindNode(const FGuid& InNodeId) const;
    virtual void PreSave(FObjectPreSaveContext SaveContext) override;
    virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
