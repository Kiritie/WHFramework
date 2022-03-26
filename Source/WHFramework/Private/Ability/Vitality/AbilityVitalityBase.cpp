	// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Vitality/AbilityVitalityBase.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Vitality/VitalityAbilityBase.h"
#include "Ability/Vitality/VitalityAttributeSetBase.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Scene/Components/WorldTextComponent.h"

	// Sets default values
AAbilityVitalityBase::AAbilityVitalityBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(FName("BoxComponent"));
	BoxComponent->SetCollisionProfileName(FName("DW_Vitality"));
	BoxComponent->SetBoxExtent(FVector(20, 20, 20));
	SetRootComponent(BoxComponent);

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponentBase>(FName("AbilitySystem"));

	AttributeSet = CreateDefaultSubobject<UVitalityAttributeSetBase>(FName("AttributeSet"));

	// states
	bDead = true;
	
	// stats
	ID = NAME_None;
	Name = TEXT("");
	RaceID = TEXT("");
	Level = 0;
	EXP = 50;
	BaseEXP = 100;
	EXPFactor = 2.f;
}

// Called when the game starts or when spawned
void AAbilityVitalityBase::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystem->InitAbilityActorInfo(this, this);

	Spawn();
}

// Called every frame
void AAbilityVitalityBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if(bDead) return;
}

void AAbilityVitalityBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.IsSaveGame())
	{
		float CurrentValue;
		if(Ar.IsLoading())
		{
			Ar << CurrentValue;
			if(CurrentValue > 0)
			{
				SetHealth(CurrentValue);
			}
			else
			{
				Revive();
				Ar << CurrentValue;
				Ar << CurrentValue;
			}
		}
		else if(Ar.IsSaveGame())
		{
			CurrentValue = GetHealth();
			Ar << CurrentValue;
		}
	}
}

void AAbilityVitalityBase::LoadData(FSaveData* InSaveData)
{
}

FSaveData* AAbilityVitalityBase::ToData(bool bSaved)
{
	static FSaveData SaveData;
	return &SaveData;
}

void AAbilityVitalityBase::ResetData(bool bRefresh)
{
	bDead = false;
	if(bRefresh) ResetData();
}

void AAbilityVitalityBase::RefreshData()
{
	HandleHealthChanged(GetHealth());
	HandleNameChanged(GetNameV());
	HandleRaceIDChanged(GetRaceID());
}

void AAbilityVitalityBase::Death(AActor* InKiller /*= nullptr*/)
{
	if (!bDead)
	{
		bDead = true;
		if(IAbilityVitalityInterface* InVitality = Cast<IAbilityVitalityInterface>(InKiller))
		{
			InVitality->ModifyEXP(GetTotalEXP());
		}
		SetEXP(0);
		SetHealth(0.f);
	}
}

void AAbilityVitalityBase::Spawn()
{
	ResetData();
	SetHealth(GetMaxHealth());
}

void AAbilityVitalityBase::Revive()
{
	if (bDead)
	{
		ResetData();
		SetHealth(GetMaxHealth());
	}
}

bool AAbilityVitalityBase::IsDead() const
{
	return bDead;
}

void AAbilityVitalityBase::SetNameV(const FString& InName)
{
	Name = InName;
	HandleNameChanged(InName);
}

void AAbilityVitalityBase::SetRaceID(const FString& InRaceID)
{
	RaceID = InRaceID;
	HandleRaceIDChanged(InRaceID);
}

void AAbilityVitalityBase::SetLevelV(int32 InLevel)
{
	Level = InLevel;
	HandleLevelChanged(InLevel);
}

void AAbilityVitalityBase::SetEXP(int32 InEXP)
{
	EXP = InEXP;
	HandleEXPChanged(InEXP);
}

int32 AAbilityVitalityBase::GetMaxEXP() const
{
	int32 MaxEXP = BaseEXP;
	for (int i = 0; i < Level - 1; i++)
	{
		MaxEXP *= EXPFactor;
	}
	return MaxEXP;
}

int32 AAbilityVitalityBase::GetTotalEXP() const
{
	int32 TotalEXP = BaseEXP;
	for (int i = 0; i < Level - 1; i++)
	{
		TotalEXP += TotalEXP * EXPFactor;
	}
	return EXP + TotalEXP - GetMaxEXP();
}

FString AAbilityVitalityBase::GetHeadInfo() const
{
	return FString::Printf(TEXT("Lv.%d \"%s\" "), Level, *Name);
}

float AAbilityVitalityBase::GetHealth() const
{
	return AttributeSet->GetHealth();
}

void AAbilityVitalityBase::SetHealth(float InValue)
{
	AbilitySystem->ApplyModToAttributeUnsafe(AttributeSet->GetHealthAttribute(), EGameplayModOp::Override, InValue);
}

float AAbilityVitalityBase::GetMaxHealth() const
{
	return AttributeSet->GetMaxHealth();
}

void AAbilityVitalityBase::SetMaxHealth(float InValue)
{
	AbilitySystem->ApplyModToAttributeUnsafe(AttributeSet->GetMaxHealthAttribute(), EGameplayModOp::Override, InValue);
}

float AAbilityVitalityBase::GetPhysicsDamage() const
{
	return AttributeSet->GetPhysicsDamage();
}

float AAbilityVitalityBase::GetMagicDamage() const
{
	return AttributeSet->GetMagicDamage();
}

UAbilitySystemComponent* AAbilityVitalityBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AAbilityVitalityBase::AddWorldText(FString InContent, EWorldTextType InContentType, EWorldTextStyle InContentStyle)
{
	auto contextHUD = NewObject<UWorldTextComponent>(this);
	contextHUD->RegisterComponent();
	contextHUD->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	contextHUD->SetRelativeLocation(FVector(0, 0, 50));
	contextHUD->SetVisibility(false);
	contextHUD->InitContent(InContent, InContentType, InContentStyle);
}

FGameplayAbilitySpecHandle AAbilityVitalityBase::AcquireAbility(TSubclassOf<UAbilityBase> InAbility, int32 InLevel /*= 1*/)
{
	if (AbilitySystem && InAbility)
	{
		FGameplayAbilitySpecDef SpecDef = FGameplayAbilitySpecDef();
		SpecDef.Ability = InAbility;
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(SpecDef, InLevel);
		return AbilitySystem->GiveAbility(AbilitySpec);
	}
	return FGameplayAbilitySpecHandle();
}

bool AAbilityVitalityBase::ActiveAbility(FGameplayAbilitySpecHandle SpecHandle, bool bAllowRemoteActivation /*= false*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->TryActivateAbility(SpecHandle, bAllowRemoteActivation);
	}
	return false;
}

bool AAbilityVitalityBase::ActiveAbilityByClass(TSubclassOf<UAbilityBase> AbilityClass, bool bAllowRemoteActivation /*= false*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->TryActivateAbilityByClass(AbilityClass, bAllowRemoteActivation);
	}
	return false;
}

bool AAbilityVitalityBase::ActiveAbilityByTag(const FGameplayTagContainer& GameplayTagContainer, bool bAllowRemoteActivation /*= false*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->TryActivateAbilitiesByTag(GameplayTagContainer, bAllowRemoteActivation);
	}
	return false;
}

void AAbilityVitalityBase::CancelAbility(UAbilityBase* Ability)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAbility(Ability);
	}
}

void AAbilityVitalityBase::CancelAbilityByHandle(const FGameplayAbilitySpecHandle& AbilityHandle)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAbilityHandle(AbilityHandle);
	}
}

void AAbilityVitalityBase::CancelAbilities(const FGameplayTagContainer& WithTags, const FGameplayTagContainer& WithoutTags, UAbilityBase* Ignore/*=nullptr*/)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAbilities(&WithTags, &WithoutTags, Ignore);
	}
}

void AAbilityVitalityBase::CancelAllAbilities(UAbilityBase* Ignore/*=nullptr*/)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAllAbilities(Ignore);
	}
}

FActiveGameplayEffectHandle AAbilityVitalityBase::ApplyEffectByClass(TSubclassOf<UGameplayEffect> EffectClass)
{
	if (AbilitySystem)
	{
		auto effectContext = AbilitySystem->MakeEffectContext();
		effectContext.AddSourceObject(this);
		auto specHandle = AbilitySystem->MakeOutgoingSpec(EffectClass, GetLevelV(), effectContext);
		if (specHandle.IsValid())
		{
			return AbilitySystem->ApplyGameplayEffectSpecToSelf(*specHandle.Data.Get());
		}
	}
	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle AAbilityVitalityBase::ApplyEffectBySpecHandle(const FGameplayEffectSpecHandle& SpecHandle)
{
	if (AbilitySystem)
	{
		return AbilitySystem->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle AAbilityVitalityBase::ApplyEffectBySpec(const FGameplayEffectSpec& Spec)
{
	if (AbilitySystem)
	{
		return AbilitySystem->ApplyGameplayEffectSpecToSelf(Spec);
	}
	return FActiveGameplayEffectHandle();
}

bool AAbilityVitalityBase::RemoveEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove/*=-1*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->RemoveActiveGameplayEffect(Handle, StacksToRemove);
	}
	return false;
}

void AAbilityVitalityBase::GetActiveAbilities(FGameplayTagContainer AbilityTags, TArray<UAbilityBase*>& ActiveAbilities)
{
	if (AbilitySystem)
	{
		AbilitySystem->GetActiveAbilitiesWithTags(AbilityTags, ActiveAbilities);
	}
}

bool AAbilityVitalityBase::GetAbilityInfo(TSubclassOf<UAbilityBase> AbilityClass, FAbilityInfo& OutAbilityInfo)
{
	if (AbilitySystem && AbilityClass != nullptr)
	{
		float Cost = 0;
		float Cooldown = 0;
		EAbilityCostType CostType = EAbilityCostType::None;
		UAbilityBase* Ability = AbilityClass.GetDefaultObject();
		if (Ability->GetCostGameplayEffect()->Modifiers.Num() > 0)
		{
			const FGameplayModifierInfo ModifierInfo = Ability->GetCostGameplayEffect()->Modifiers[0];
			ModifierInfo.ModifierMagnitude.GetStaticMagnitudeIfPossible(1, Cost);
			if (ModifierInfo.Attribute == AttributeSet->GetHealthAttribute())
			{
				CostType = EAbilityCostType::Health;
			}
		}
		Ability->GetCooldownGameplayEffect()->DurationMagnitude.GetStaticMagnitudeIfPossible(1, Cooldown);
		OutAbilityInfo = FAbilityInfo(CostType, Cost, Cooldown);
		return true;
	}
	return false;
}

void AAbilityVitalityBase::ModifyHealth(float InDeltaValue)
{
	AbilitySystem->ApplyModToAttributeUnsafe(AttributeSet->GetHealthAttribute(), EGameplayModOp::Additive, InDeltaValue);
}

void AAbilityVitalityBase::ModifyEXP(float InDeltaValue)
{
	const int32 MaxEXP = GetMaxEXP();
	EXP += InDeltaValue;
	if (InDeltaValue > 0.f)
	{
		if (EXP >= MaxEXP)
		{
			Level++;
			EXP -= MaxEXP;
		}
	}
	else
	{
		if (EXP < 0.f)
		{
			EXP = 0.f;
		}
	}
	HandleEXPChanged(EXP);
}

void AAbilityVitalityBase::HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	if (GetHealth() <= 0.f)
	{
		Death(SourceActor);
	}
}

void AAbilityVitalityBase::HandleNameChanged(const FString& NewValue)
{
	
}

void AAbilityVitalityBase::HandleRaceIDChanged(const FString& NewValue)
{
	
}

void AAbilityVitalityBase::HandleLevelChanged(int32 NewValue, int32 DeltaValue /*= 0*/)
{
	
}

void AAbilityVitalityBase::HandleEXPChanged(int32 NewValue, int32 DeltaValue /*= 0*/)
{
	
}

void AAbilityVitalityBase::HandleHealthChanged(float NewValue, float DeltaValue /*= 0.f*/)
{
	if(DeltaValue > 0.f)
	{
		AddWorldText(FString::FromInt(FMath::Abs(DeltaValue)), EWorldTextType::HealthRecover, DeltaValue < GetMaxHealth() ? EWorldTextStyle::Normal : EWorldTextStyle::Stress);
	}
}

void AAbilityVitalityBase::HandleMaxHealthChanged(float NewValue, float DeltaValue /*= 0.f*/)
{
	
}
