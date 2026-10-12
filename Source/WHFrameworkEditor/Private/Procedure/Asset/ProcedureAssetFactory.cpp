#include "Procedure/Asset/ProcedureAssetFactory.h"

#include "Procedure/Base/ProcedureAsset.h"

UProcedureAssetFactory::UProcedureAssetFactory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SupportedClass = UProcedureAsset::StaticClass();
    ParentClass = UProcedureAsset::StaticClass();
    PickedClass = UProcedureAsset::StaticClass();
}

UObject* UProcedureAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
    return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn);
}

UObject* UProcedureAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
    UClass* AssetClass = PickedClass ? PickedClass.Get() : Class;
    if (!AssetClass || !AssetClass->IsChildOf(UProcedureAsset::StaticClass()) ||
        AssetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
    {
        return nullptr;
    }
    UProcedureAsset* Asset = NewObject<UProcedureAsset>(InParent, AssetClass, Name, Flags | RF_Transactional);
    Asset->StartId = FGuid::NewGuid();
    Asset->Recompile();
    return Asset;
}
