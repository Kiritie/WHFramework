#include "ObjectPool/ObjectPoolModule.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "WHFrameworkCoreMacros.h"
#include "Common/LifecycleTypes.h"
#include "Common/CommonTypes.h"
#include "Common/Base/WHActor.h"
#include "Pawn/Base/PawnBase.h"
#include "Character/Base/CharacterBase.h"
#include "Ability/Spawner/AbilitySpawnerBase.h"
#include "Scene/SceneModule.h"
#include "Scene/Actor/SceneActorInterface.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "ObjectPool/ObjectPoolBucket.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "ObjectPool/ObjectPoolProviderInterface.h"
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
}

#if WITH_EDITOR
void UObjectPoolModule::OnGenerate()
{
	Super::OnGenerate();
}

void UObjectPoolModule::OnDestroy()
{
	Super::OnDestroy();
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
		ClearAllObjectForCleanup();
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
	if (!IsBusinessAdmissionOpen()) return nullptr;
	if (!::IsValid(InClass) || InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) return nullptr;
	if (const UScriptStruct* SuppliedType = InParam.GetStructType(); SuppliedType && SuppliedType->IsChildOf(FSpawnParameter::StaticStruct()))
	{
		const UScriptStruct* ExpectedType = FObjectPoolSpawnParameterRegistry::Resolve(InClass);
		if (!ExpectedType || !SuppliedType->IsChildOf(ExpectedType))
		{
			UE_LOG(LogTemp, Warning, TEXT("对象池拒绝创建 %s：生成参数类型 %s 与要求的 %s 不匹配。"),
				*GetNameSafe(InClass), *GetNameSafe(SuppliedType), *GetNameSafe(ExpectedType));
			return nullptr;
		}
	}
	IObjectPoolProvider* Provider = ResolveProvider(InClass);
	UObject* Object = Provider ? Provider->Spawn(InClass, InParam) : nullptr;
	if(!Object)
	{
		return nullptr;
	}
	InactiveObjects.Remove(Object);
	AActor* Actor = Cast<AActor>(Object);
	auto FinishActorSpawn = [&]()
	{
		if (ActorProvider->TakeDeferredSpawn(Actor))
		{
			const FActorSpawnParameter* Parameter = InParam.GetPtr<FActorSpawnParameter>();
			Actor->FinishSpawning(Parameter ? Parameter->Transform : FTransform::Identity);
		}
	};
	auto HasValidActorInitialization = [&]()
	{
		if (!Actor->HasActorBegunPlay() || !Actor->Implements<UWHActorInterface>()) return true;
		const bool bDefaultLifecycle = IWHActorInterface::Execute_IsUseDefaultLifecycle(Actor);
		const bool bInitialized = !bDefaultLifecycle || IWHActorInterface::Execute_IsInitialized(Actor);
		return bInitialized;
	};
	if (Actor && Actor->HasAuthority() && (Actor->IsA<AWHActor>() || Actor->IsA<APawnBase>() ||
		Actor->IsA<ACharacterBase>() || Actor->IsA<AAbilitySpawnerBase>()))
	{
		const FWHActorSpawnParameter* Parameter = InParam.GetPtr<FWHActorSpawnParameter>();
		const FGuid NewId = Parameter && Parameter->bOverrideActorID ? Parameter->ActorID : FGuid::NewGuid();
		USceneModule* Scene = USceneModule::GetPtr(Actor->GetWorld());
		const bool bAssigned = Scene && ISceneActorInterface::Execute_SetActorID(Actor, NewId) &&
			ISceneActorInterface::Execute_GetActorID(Actor) == NewId;
		if (bAssigned) FinishActorSpawn();
		if (!bAssigned || !HasValidActorInitialization() || !Scene->AddSceneActor(Actor))
		{
			UE_LOG(LogTemp, Error, TEXT("对象池无法分配场景实体身份：%s id=%s"), *GetNameSafe(Actor), *NewId.ToString());
			if (Scene) Scene->RemoveSceneActor(Actor);
			ActorProvider->Destroy(Object);
			return nullptr;
		}
	}
	else if (Actor)
	{
		FinishActorSpawn();
		if (!HasValidActorInitialization())
		{
			ActorProvider->Destroy(Object);
			return nullptr;
		}
	}
	if(Object->Implements<UObjectPoolInterface>())
	{
		IObjectPoolInterface::Execute_OnSpawn(Object, InParam);
	}
	return Object;
}

void UObjectPoolModule::DespawnObject(UObject* InObject, EObjectDespawnMode InMode)
{
	if (!IsBusinessAdmissionOpen()) return;
	DespawnObjectForCleanup(InObject, InMode);
}

void UObjectPoolModule::DespawnObjectForCleanup(UObject* InObject, EObjectDespawnMode InMode)
{
	if(!::IsValid(InObject) || ModuleState == EModuleState::Terminated)
	{
		return;
	}
	if (UObjectPoolModule* OwnerPool = InObject->GetTypedOuter<UObjectPoolModule>(); OwnerPool && OwnerPool != this) return;
	if (!IsBusinessAdmissionOpen()) InMode = EObjectDespawnMode::Destroy;
	if(InMode == EObjectDespawnMode::Destroy)
	{
		DestroyObjectForCleanup(InObject);
		return;
	}
	if(!ensureMsgf(!IsInactive(InObject), TEXT("Object %s was despawned more than once."), *GetNameSafe(InObject)))
	{
		return;
	}
	if (RetiringObjects.Contains(InObject)) return;
	RetiringObjects.Add(InObject);
	ON_SCOPE_EXIT { RetiringObjects.Remove(InObject); };

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
	if (!IsBusinessAdmissionOpen()) return;
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
	if (!IsBusinessAdmissionOpen()) return;
	ClearAllObjectForCleanup();
}

void UObjectPoolModule::ClearAllObjectForCleanup()
{
	TArray<TWeakObjectPtr<UObject>> ObjectsToDestroy;
	for(const TWeakObjectPtr<UObject>& Object : InactiveObjects)
	{
		if(Object.IsValid())
		{
			ObjectsToDestroy.Emplace(Object.Get());
		}
	}
	for(const TWeakObjectPtr<UObject>& Entry : ObjectsToDestroy)
	{
		UObject* Object = Entry.Get();
		if (!::IsValid(Object) || InactiveObjects.Remove(Object) == 0) continue;
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
	if (!IsBusinessAdmissionOpen()) return;
	if(InClass)
	{
		PoolPolicies.Add(InClass, InPolicy);
	}
}

void UObjectPoolModule::DestroyObject(UObject* InObject)
{
	if (!IsBusinessAdmissionOpen()) return;
	DestroyObjectForCleanup(InObject);
}

void UObjectPoolModule::DestroyObjectForCleanup(UObject* InObject)
{
	if(!::IsValid(InObject))
	{
		return;
	}
	if (RetiringObjects.Contains(InObject)) return;
	RetiringObjects.Add(InObject);
	ON_SCOPE_EXIT { RetiringObjects.Remove(InObject); };

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
