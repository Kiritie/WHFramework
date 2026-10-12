#pragma once

#include "Common/Asset/AssetFactoryBase.h"
#include "ProcedureAssetFactory.generated.h"

UCLASS(hidecategories = Object, MinimalAPI)
class UProcedureAssetFactory : public UAssetFactoryBase
{
    GENERATED_BODY()

public:
    UProcedureAssetFactory(const FObjectInitializer& ObjectInitializer);
    virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
    virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};
