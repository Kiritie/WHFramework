#pragma once

#include "CoreMinimal.h"
#include "Setting/SettingPresentationTypes.h"
#include "Parameter/ParameterTypes.h"
#include "Setting/SettingModuleTypes.h"

class FProperty;
class UStruct;

struct FResolvedSettingProperty
{
    void* ContainerPtr = nullptr;
    FProperty* Property = nullptr;
};

class FSettingPropertyAccess
{
public:
    static bool IsSupported(const FProperty* InProperty);
    static ESettingRendererType InferRenderer(const FProperty* InProperty);
    static bool Resolve(UStruct* InRootStruct, void* InRootData, const FString& InPath, FResolvedSettingProperty& OutResolved);
    static FParameter Read(const FResolvedSettingProperty& InResolved);
    static bool Write(const FResolvedSettingProperty& InResolved, const FParameter& InValue);
};
