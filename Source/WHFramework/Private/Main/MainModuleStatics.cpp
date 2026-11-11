#include "Main/MainModuleStatics.h"

#include "Engine/Engine.h"
#include "Gameplay/WHGameInstance.h"
#include "Main/MainModule.h"

namespace
{
	UWorld* ResolveWorld(const UObject* Context)
	{
		return GEngine && Context ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
	}
}

UModuleBase* UMainModuleStatics::GetModuleByClassInWorld(UWorld* InWorld, TSubclassOf<UModuleBase> InClass)
{
	AMainModule* MainModule = InWorld && InClass ? AMainModule::GetPtr(InWorld) : nullptr;
	if (!MainModule || !MainModule->CanResolveModules()) return nullptr;
	const FName Name = InClass.GetDefaultObject()->GetModuleName();
	UModuleBase* const* Entry = MainModule->GetModuleMap().Find(Name);
	UModuleBase* Module = Entry ? *Entry : nullptr;
	return Module && Module->IsBusinessAdmissionOpen() && Module->IsA(InClass) ? Module : nullptr;
}

UModuleBase* UMainModuleStatics::GetModuleByClassForCleanup(UWorld* InWorld, TSubclassOf<UModuleBase> InClass)
{
	AMainModule* MainModule = InWorld && InClass ? AMainModule::GetPtr(InWorld) : nullptr;
	if (!MainModule || (!MainModule->IsTerminationInProgress() && !MainModule->CanResolveModules())) return nullptr;
	const FName Name = InClass.GetDefaultObject()->GetModuleName();
	UModuleBase* const* Entry = MainModule->GetModuleMap().Find(Name);
	UModuleBase* Module = Entry ? *Entry : nullptr;
	return Module && Module->IsA(InClass) ? Module : nullptr;
}

#define DEFINE_MODULE_ACTION(Action) \
void UMainModuleStatics::Action##ModuleByClass(const UObject* Context, TSubclassOf<UModuleBase> Class) \
{ \
	if (UModuleBase* Module = GetModuleByClass(Context, Class)) Module->Action(); \
} \
void UMainModuleStatics::Action##ModuleByName(const UObject* Context, FName Name) \
{ \
	if (UModuleBase* Module = GetModuleByName(Context, Name)) Module->Action(); \
}

DEFINE_MODULE_ACTION(Run)
DEFINE_MODULE_ACTION(Reset)
DEFINE_MODULE_ACTION(Pause)
DEFINE_MODULE_ACTION(UnPause)
DEFINE_MODULE_ACTION(Termination)
#undef DEFINE_MODULE_ACTION

void UMainModuleStatics::PauseAllModule(const UObject* Context)
{
	if (AMainModule* Main = GetMainModule(Context)) Main->PauseModules();
}

void UMainModuleStatics::UnPauseAllModule(const UObject* Context)
{
	if (AMainModule* Main = GetMainModule(Context)) Main->UnPauseModules();
}

AMainModule* UMainModuleStatics::GetMainModule(const UObject* Context)
{
	return AMainModule::GetPtr(ResolveWorld(Context));
}

TArray<UModuleBase*> UMainModuleStatics::GetAllModule(const UObject* Context)
{
	const AMainModule* Main = GetMainModule(Context);
	return Main && Main->CanResolveModules() ? Main->GetModules() : TArray<UModuleBase*>();
}

bool UMainModuleStatics::IsExistModuleByClass(const UObject* Context, TSubclassOf<UModuleBase> Class)
{
	return GetModuleByClass(Context, Class) != nullptr;
}

bool UMainModuleStatics::IsExistModuleByName(const UObject* Context, FName Name)
{
	return GetModuleByName(Context, Name) != nullptr;
}

UModuleBase* UMainModuleStatics::GetModuleByClass(const UObject* Context, TSubclassOf<UModuleBase> Class)
{
	return GetModuleByClassInWorld(ResolveWorld(Context), Class);
}

UModuleBase* UMainModuleStatics::GetGameInstanceModuleByClass(const UGameInstance* InGameInstance, TSubclassOf<UModuleBase> InClass)
{
	const UWHGameInstance* GameInstance = Cast<UWHGameInstance>(InGameInstance);
	return GameInstance ? GameInstance->FindModuleByClass(InClass) : nullptr;
}

UModuleBase* UMainModuleStatics::GetModuleByName(const UObject* Context, FName Name)
{
	const AMainModule* Main = GetMainModule(Context);
	if (!Main || !Main->CanResolveModules()) return nullptr;
	UModuleBase* const* Entry = Main->GetModuleMap().Find(Name);
	return Entry && *Entry && (*Entry)->IsBusinessAdmissionOpen() ? *Entry : nullptr;
}

UModuleNetworkComponentBase* UMainModuleStatics::GetModuleNetworkComponentByClass(APlayerController* Controller, TSubclassOf<UModuleNetworkComponentBase> Class)
{
	return AMainModule::GetModuleNetworkComponentByClass<UModuleNetworkComponentBase>(Controller, Class);
}
