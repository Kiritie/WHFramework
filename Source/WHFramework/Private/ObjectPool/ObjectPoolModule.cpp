#include "ObjectPool/ObjectPoolModule.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "ObjectPool/ObjectPoolBucket.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "ObjectPool/ObjectPoolProvider.h"
#include "ObjectPool/Provider/ActorPoolProvider.h"
#include "ObjectPool/Provider/ObjectPoolProvider.h"
#include "ObjectPool/Provider/WidgetPoolProvider.h"

IMPLEMENTATION_MODULE(UObjectPoolModule)

UObjectPoolModule::UObjectPoolModule()
{
	ModuleName = FName("ObjectPoolModule");
	ModuleDisplayName = FText::FromString(TEXT("Object Pool Module"));
	bModuleRequired = true;
}

UObjectPoolModule::~UObjectPoolModule()
{
	TERMINATION_MODULE(UObjectPoolModule)
}

#if WITH_EDITOR
void UObjectPoolModule::OnGenerate()
{
	Super::OnGenerate();
}

void UObjectPoolModule::OnDestroy()
{
	Super::OnDestroy();
	TERMINATION_MODULE(UObjectPoolModule)
}
#endif

void UObjectPoolModule::OnInitialize()
{
	Super::OnInitialize();
	ObjectProvider = MakeUnique<FObjectPoolProvider>(*this);
	ActorProvider = MakeUnique<FActorPoolProvider>(*this);
	WidgetProvider = MakeUnique<FWidgetPoolProvider>(*this);
}

void UObjectPoolModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UObjectPoolModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
}

void UObjectPoolModule::OnPause()
{
	Super::OnPause();
}

void UObjectPoolModule::OnUnPause()
{
	Super::OnUnPause();
}

void UObjectPoolModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
	if(PHASEC(InPhase, EPhase::Final))
	{
		ClearAllObject();
		WidgetProvider.Reset();
		ActorProvider.Reset();
		ObjectProvider.Reset();
	}
}

FString UObjectPoolModule::GetModuleDebugMessage()
{
	FString DebugMessage;
	for(const UObjectPoolBucket* Bucket : GenericBuckets)
	{
		if(Bucket && Bucket->Num() > 0)
		{
			DebugMessage.Appendf(TEXT("Generic: %d\n"), Bucket->Num());
		}
	}
	DebugMessage.RemoveFromEnd(TEXT("\n"));
	return DebugMessage.IsEmpty() ? Super::GetModuleDebugMessage() : DebugMessage;
}

UObject* UObjectPoolModule::SpawnObject(UClass* InClass, const FParameter& InParam)
{
	IObjectPoolProvider* Provider = ResolveProvider(InClass);
	UObject* Object = Provider ? Provider->Spawn(InClass, InParam) : nullptr;
	if(!Object)
	{
		return nullptr;
	}

	InactiveObjects.Remove(Object);
	if(Object->Implements<UObjectPoolInterface>())
	{
		IObjectPoolInterface::Execute_OnSpawn(Object, InParam);
	}
	return Object;
}

void UObjectPoolModule::DespawnObject(UObject* InObject, EObjectDespawnMode InMode)
{
	if(!::IsValid(InObject) || ModuleState == EModuleState::Terminated)
	{
		return;
	}
	if(InMode == EObjectDespawnMode::Destroy)
	{
		DestroyObject(InObject);
		return;
	}
	if(!ensureMsgf(!IsInactive(InObject), TEXT("Object %s was despawned more than once."), *GetNameSafe(InObject)))
	{
		return;
	}

	IObjectPoolProvider* Provider = ResolveProvider(InObject->GetClass());
	if(Provider)
	{
		Provider->PrepareDespawn(InObject);
	}

	const bool bRecovery = Provider && Provider->Despawn(InObject);
	const EObjectDespawnMode FinalMode = bRecovery
		? EObjectDespawnMode::Recovery
		: EObjectDespawnMode::Destroy;
	if(InObject->Implements<UObjectPoolInterface>())
	{
		IObjectPoolInterface::Execute_OnDespawn(InObject, FinalMode);
	}

	if(bRecovery)
	{
		InactiveObjects.Add(InObject);
	}
}

void UObjectPoolModule::DespawnObjects(const TArray<UObject*>& InObjects)
{
	for(UObject* Object : InObjects)
	{
		DespawnObject(Object);
	}
}

void UObjectPoolModule::ClearObject(TSubclassOf<UObject> InClass)
{
	TArray<UObject*> ObjectsToDestroy;
	for(const TWeakObjectPtr<UObject>& Object : InactiveObjects)
	{
		if(Object.IsValid() && Object->GetClass() == InClass)
		{
			ObjectsToDestroy.Add(Object.Get());
		}
	}
	for(UObject* Object : ObjectsToDestroy)
	{
		DestroyObject(Object);
	}
}

void UObjectPoolModule::ClearAllObject()
{
	TArray<UObject*> ObjectsToDestroy;
	for(const TWeakObjectPtr<UObject>& Object : InactiveObjects)
	{
		if(Object.IsValid())
		{
			ObjectsToDestroy.Add(Object.Get());
		}
	}
	for(UObject* Object : ObjectsToDestroy)
	{
		if(IObjectPoolProvider* Provider = ResolveProvider(Object->GetClass()))
		{
			Provider->Destroy(Object);
		}
	}

	if(WidgetProvider)
	{
		WidgetProvider->ClearAll();
	}
	if(ActorProvider)
	{
		ActorProvider->ClearAll();
	}
	if(ObjectProvider)
	{
		ObjectProvider->ClearAll();
	}

	for(UObjectPoolWidgetBucket* Bucket : WidgetBuckets)
	{
		if(Bucket)
		{
			Bucket->Clear();
		}
	}
	for(UObjectPoolBucket* Bucket : GenericBuckets)
	{
		if(Bucket)
		{
			Bucket->Clear();
		}
	}

	InactiveObjects.Reset();
	WidgetBuckets.Reset();
	GenericBuckets.Reset();
}

FObjectPoolPolicy UObjectPoolModule::GetPoolPolicy(TSubclassOf<UObject> InClass) const
{
	for(UClass* Class = InClass.Get(); Class; Class = Class->GetSuperClass())
	{
		if(const FObjectPoolPolicy* Policy = PoolPolicies.Find(Class))
		{
			return *Policy;
		}
	}
	return FObjectPoolPolicyRegistry::Resolve(InClass);
}

void UObjectPoolModule::SetPoolPolicy(TSubclassOf<UObject> InClass, const FObjectPoolPolicy& InPolicy)
{
	if(InClass)
	{
		PoolPolicies.Add(InClass, InPolicy);
	}
}

void UObjectPoolModule::DestroyObject(UObject* InObject)
{
	if(!::IsValid(InObject))
	{
		return;
	}

	const bool bWasInactive = InactiveObjects.Remove(InObject) > 0;
	if(!bWasInactive && InObject->Implements<UObjectPoolInterface>())
	{
		IObjectPoolInterface::Execute_OnDespawn(InObject, EObjectDespawnMode::Destroy);
	}

	if(IObjectPoolProvider* Provider = ResolveProvider(InObject->GetClass()))
	{
		Provider->Destroy(InObject);
	}
}

UObjectPoolBucket* UObjectPoolModule::FindOrAddGenericBucket(UClass* InClass, UObject* InScope, int32 InMaxIdle)
{
	for(UObjectPoolBucket* Bucket : GenericBuckets)
	{
		if(Bucket && Bucket->Matches(InClass, InScope))
		{
			return Bucket;
		}
	}

	UObjectPoolBucket* Bucket = NewObject<UObjectPoolBucket>(this);
	Bucket->Initialize(InClass, InScope, InMaxIdle);
	GenericBuckets.Add(Bucket);
	return Bucket;
}

UObjectPoolWidgetBucket* UObjectPoolModule::FindOrAddWidgetBucket(
	UObject* InScope,
	UWorld* InWorld,
	APlayerController* InPlayerController)
{
	for(UObjectPoolWidgetBucket* Bucket : WidgetBuckets)
	{
		if(Bucket && Bucket->Matches(InScope))
		{
			return Bucket;
		}
	}

	UObjectPoolWidgetBucket* Bucket = NewObject<UObjectPoolWidgetBucket>(this);
	Bucket->Initialize(InScope, InWorld, InPlayerController);
	WidgetBuckets.Add(Bucket);
	return Bucket;
}

void UObjectPoolModule::RemoveFromGenericBucket(UObject* InObject)
{
	for(UObjectPoolBucket* Bucket : GenericBuckets)
	{
		if(Bucket && Bucket->Remove(InObject))
		{
			return;
		}
	}
}

IObjectPoolProvider* UObjectPoolModule::ResolveProvider(UClass* InClass) const
{
	if(!InClass)
	{
		return nullptr;
	}
	if(InClass->IsChildOf<UUserWidget>())
	{
		return WidgetProvider.Get();
	}
	if(InClass->IsChildOf<AActor>())
	{
		return ActorProvider.Get();
	}
	return ObjectProvider.Get();
}

bool UObjectPoolModule::IsInactive(UObject* InObject) const
{
	return InactiveObjects.Contains(InObject);
}
