// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Character/AbilityCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "Ability/Base/AbilityBase.h"
#include "Ability/Character/CharacterAttributeSetBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Sight.h"
#include "Scene/SceneModuleBPLibrary.h"
#include "Scene/Components/WorldTextComponent.h"

//////////////////////////////////////////////////////////////////////////
// AAbilityCharacterBase

AAbilityCharacterBase::AAbilityCharacterBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponentBase>(FName("AbilitySystem"));

	AttributeSet = CreateDefaultSubobject<UCharacterAttributeSetBase>(FName("AttributeSet"));

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96);
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("DW_Character"));
	GetCapsuleComponent()->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;

	GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -90), FRotator(0, -90, 0));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCastShadow(false);

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0, 360, 0);
	GetCharacterMovement()->JumpZVelocity = 420;
	GetCharacterMovement()->AirControl = 0.2f;
	GetCharacterMovement()->bComponentShouldUpdatePhysicsVolume = false;

	// states
	bDead = true;

	// stats
	Name = TEXT("");
	RaceID = TEXT("");
	Level = 0;
	EXP = 0;
	BaseEXP = 100;
	EXPFactor = 2.f;

	MovementRate = 1;
	RotationRate = 1;

	// local
	DefaultAbility = FAbilityData();

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	AutoPossessAI = EAutoPossessAI::Disabled;
}

// Called when the game starts or when spawned
void AAbilityCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	DefaultGravityScale = GetCharacterMovement()->GravityScale;
	DefaultAirControl = GetCharacterMovement()->AirControl;

	AbilitySystem->InitAbilityActorInfo(this, this);

	Spawn();
}

// Called every frame
void AAbilityCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bDead) return;
}

void AAbilityCharacterBase::Serialize(FArchive& Ar)
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

void AAbilityCharacterBase::LoadData(FSaveData* InSaveData)
{
}

FSaveData* AAbilityCharacterBase::ToData(bool bSaved)
{
	static FSaveData SaveData = FSaveData();
	return &SaveData;
}

void AAbilityCharacterBase::ResetData(bool bRefresh)
{
	// states
	bDead = false;

	// stats
	SetMotionRate(1, 1);
			
	if(bRefresh) RefreshData();
}

void AAbilityCharacterBase::RefreshData()
{
	HandleEXPChanged(GetEXP());
	HandleLevelChanged(GetLevelV());
	HandleHealthChanged(GetHealth());
	HandleMoveSpeedChanged(GetMoveSpeed());
}

void AAbilityCharacterBase::Spawn()
{
	SetVisible(true);
	SetHealth(GetMaxHealth());
}

void AAbilityCharacterBase::Revive()
{
	if (bDead)
	{
		SetVisible(true);
		SetHealth(GetMaxHealth());
	}
}

void AAbilityCharacterBase::Death(AActor* InKiller /*= nullptr*/)
{
	if (!IsDead())
	{
		if(IAbilityVitalityInterface* InVitality = Cast<IAbilityVitalityInterface>(InKiller))
		{
			InVitality->ModifyEXP(GetTotalEXP());
		}
		SetEXP(0);
		SetHealth(0.f);
		OnCharacterDead.Broadcast();
	}
}

void AAbilityCharacterBase::Jump()
{
	Super::Jump();
}

void AAbilityCharacterBase::UnJump()
{
	Super::StopJumping();
}

FGameplayAbilitySpecHandle AAbilityCharacterBase::AcquireAbility(TSubclassOf<UAbilityBase> InAbility, int32 InLevel /*= 1*/)
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

bool AAbilityCharacterBase::ActiveAbility(FGameplayAbilitySpecHandle SpecHandle, bool bAllowRemoteActivation /*= false*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->TryActivateAbility(SpecHandle, bAllowRemoteActivation);
	}
	return false;
}

bool AAbilityCharacterBase::ActiveAbilityByClass(TSubclassOf<UAbilityBase> AbilityClass, bool bAllowRemoteActivation /*= false*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->TryActivateAbilityByClass(AbilityClass, bAllowRemoteActivation);
	}
	return false;
}

bool AAbilityCharacterBase::ActiveAbilityByTag(const FGameplayTagContainer& AbilityTags, bool bAllowRemoteActivation /*= false*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->TryActivateAbilitiesByTag(AbilityTags, bAllowRemoteActivation);
	}
	return false;
}

void AAbilityCharacterBase::CancelAbility(UAbilityBase* Ability)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAbility(Ability);
	}
}

void AAbilityCharacterBase::CancelAbilityByHandle(const FGameplayAbilitySpecHandle& AbilityHandle)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAbilityHandle(AbilityHandle);
	}
}

void AAbilityCharacterBase::CancelAbilities(const FGameplayTagContainer& WithTags, const FGameplayTagContainer& WithoutTags, UAbilityBase* Ignore/*=nullptr*/)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAbilities(&WithTags, &WithoutTags, Ignore);
	}
}

void AAbilityCharacterBase::CancelAllAbilities(UAbilityBase* Ignore/*=nullptr*/)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAllAbilities(Ignore);
	}
}

FActiveGameplayEffectHandle AAbilityCharacterBase::ApplyEffectByClass(TSubclassOf<UGameplayEffect> EffectClass)
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

FActiveGameplayEffectHandle AAbilityCharacterBase::ApplyEffectBySpecHandle(const FGameplayEffectSpecHandle& SpecHandle)
{
	if (AbilitySystem)
	{
		return AbilitySystem->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle AAbilityCharacterBase::ApplyEffectBySpec(const FGameplayEffectSpec& Spec)
{
	if (AbilitySystem)
	{
		return AbilitySystem->ApplyGameplayEffectSpecToSelf(Spec);
	}
	return FActiveGameplayEffectHandle();
}

bool AAbilityCharacterBase::RemoveEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove/*=-1*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->RemoveActiveGameplayEffect(Handle, StacksToRemove);
	}
	return false;
}

void AAbilityCharacterBase::GetActiveAbilities(FGameplayTagContainer AbilityTags, TArray<UAbilityBase*>& ActiveAbilities)
{
	if (AbilitySystem)
	{
		AbilitySystem->GetActiveAbilitiesWithTags(AbilityTags, ActiveAbilities);
	}
}

bool AAbilityCharacterBase::GetAbilityInfo(TSubclassOf<UAbilityBase> AbilityClass, FAbilityInfo& OutAbilityInfo)
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

void AAbilityCharacterBase::ModifyEXP(float InDeltaValue)
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

void AAbilityCharacterBase::ModifyHealth(float InDeltaValue)
{
	if(InDeltaValue < 0.f && GetHealth() > 0.f || InDeltaValue > 0.f && GetHealth() < GetMaxHealth())
	{
		AbilitySystem->ApplyModToAttributeUnsafe(AttributeSet->GetHealthAttribute(), EGameplayModOp::Additive, InDeltaValue);
	}
}

void AAbilityCharacterBase::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
}

UAbilitySystemComponent* AAbilityCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AAbilityCharacterBase::SetNameV(const FString& InName)
{
	Name = InName;
	HandleNameChanged(InName);
}

void AAbilityCharacterBase::SetRaceID(const FString& InRaceID)
{
	RaceID = InRaceID;
	HandleRaceIDChanged(InRaceID);
}

void AAbilityCharacterBase::SetLevelV(int32 InLevel)
{
	Level = InLevel;
	HandleLevelChanged(InLevel);
}

void AAbilityCharacterBase::SetEXP(int32 InEXP)
{
	EXP = InEXP;
	HandleEXPChanged(InEXP);
}

int32 AAbilityCharacterBase::GetMaxEXP() const
{
	int32 MaxEXP = BaseEXP;
	for (int i = 0; i < Level - 1; i++)
	{
		MaxEXP *= EXPFactor;
	}
	return MaxEXP;
}

int32 AAbilityCharacterBase::GetTotalEXP() const
{
	int32 TotalEXP = BaseEXP;
	for (int i = 0; i < Level - 1; i++)
	{
		TotalEXP += TotalEXP * EXPFactor;
	}
	return EXP + TotalEXP - GetMaxEXP();
}

FString AAbilityCharacterBase::GetHeadInfo() const
{
	return FString::Printf(TEXT("Lv.%d \"%s\" "), Level, *Name);
}

float AAbilityCharacterBase::GetHealth() const
{
	return AttributeSet->GetHealth();
}

void AAbilityCharacterBase::SetHealth(float InValue)
{
	AbilitySystem->ApplyModToAttributeUnsafe(AttributeSet->GetHealthAttribute(), EGameplayModOp::Override, InValue);
}

float AAbilityCharacterBase::GetMaxHealth() const
{
	return AttributeSet->GetMaxHealth();
}

void AAbilityCharacterBase::SetMaxHealth(float InValue)
{
	AbilitySystem->ApplyModToAttributeUnsafe(AttributeSet->GetMaxHealthAttribute(), EGameplayModOp::Override, InValue);
}

float AAbilityCharacterBase::GetMoveSpeed() const
{
	return AttributeSet->GetMoveSpeed();
}

void AAbilityCharacterBase::SetMoveSpeed(float InValue)
{
	AbilitySystem->ApplyModToAttributeUnsafe(AttributeSet->GetMoveSpeedAttribute(), EGameplayModOp::Override, InValue);
}

float AAbilityCharacterBase::GetRotationSpeed() const
{
	return AttributeSet->GetRotationSpeed();
}

void AAbilityCharacterBase::SetRotationSpeed(float InValue)
{
	AbilitySystem->ApplyModToAttributeUnsafe(AttributeSet->GetRotationSpeedAttribute(), EGameplayModOp::Override, InValue);
}

float AAbilityCharacterBase::GetJumpForce() const
{
	return AttributeSet->GetJumpForce();
}

void AAbilityCharacterBase::SetJumpForce(float InValue)
{
	AbilitySystem->ApplyModToAttributeUnsafe(AttributeSet->GetJumpForceAttribute(), EGameplayModOp::Override, InValue);
}

float AAbilityCharacterBase::GetPhysicsDamage() const
{
	return AttributeSet->GetPhysicsDamage();
}

float AAbilityCharacterBase::GetMagicDamage() const
{
	return AttributeSet->GetMagicDamage();
}

FVector AAbilityCharacterBase::GetMoveVelocity(bool bIgnoreZ) const
{
	FVector Velocity = GetMovementComponent()->Velocity;
	if(bIgnoreZ) Velocity.Z = 0;
	return Velocity;
}

FVector AAbilityCharacterBase::GetMoveDirection(bool bIgnoreZ) const
{
	return GetMoveVelocity(bIgnoreZ).GetSafeNormal();
}

float AAbilityCharacterBase::GetRadius() const
{
	if(!GetCapsuleComponent()) return 0.f;

	return GetCapsuleComponent()->GetScaledCapsuleRadius();
}

float AAbilityCharacterBase::GetHalfHeight() const
{
	if(!GetCapsuleComponent()) return 0.f;

	return GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

void AAbilityCharacterBase::AddWorldText(FString InContent, EWorldTextType InContentType, EWorldTextStyle InContentStyle)
{
	auto contextHUD = NewObject<UWorldTextComponent>(this);
	contextHUD->RegisterComponent();
	contextHUD->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	contextHUD->SetRelativeLocation(FVector(0, 0, 0));
	contextHUD->SetVisibility(false);
	contextHUD->InitContent(InContent, InContentType, InContentStyle);
}

float AAbilityCharacterBase::Distance(AAbilityCharacterBase* InTargetCharacter, bool bIgnoreRadius /*= true*/, bool bIgnoreZAxis /*= true*/)
{
	if(!InTargetCharacter || !InTargetCharacter->IsValidLowLevel()) return -1;
	return FVector::Distance(FVector(GetActorLocation().X, GetActorLocation().Y, bIgnoreZAxis ? 0 : GetActorLocation().Z), FVector(InTargetCharacter->GetActorLocation().X, InTargetCharacter->GetActorLocation().Y, bIgnoreZAxis ? 0 : InTargetCharacter->GetActorLocation().Z)) - (bIgnoreRadius ? 0 : InTargetCharacter->GetRadius());
}

void AAbilityCharacterBase::SetVisible_Implementation(bool bVisible)
{
	GetRootComponent()->SetVisibility(bVisible, true);
}

void AAbilityCharacterBase::SetMotionRate_Implementation(float InMovementRate, float InRotationRate)
{
	MovementRate = InMovementRate;
	RotationRate = InRotationRate;
	HandleMoveSpeedChanged(GetMoveSpeed());
	HandleRotationSpeedChanged(GetRotationSpeed());
}

void AAbilityCharacterBase::HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	if (GetHealth() <= 0.f)
	{
		Death(SourceActor);
	}
}

void AAbilityCharacterBase::HandleNameChanged(const FString& NewValue)
{
}

void AAbilityCharacterBase::HandleRaceIDChanged(const FString& NewValue)
{
}

void AAbilityCharacterBase::HandleLevelChanged(int32 NewValue, int32 DeltaValue /*= 0*/)
{
	DefaultAbility.AbilityLevel = NewValue;
}

void AAbilityCharacterBase::HandleEXPChanged(int32 NewValue, int32 DeltaValue /*= 0*/)
{

}

void AAbilityCharacterBase::HandleHealthChanged(float NewValue, float DeltaValue /*= 0.f*/)
{
	if(DeltaValue > 0.f)
	{
		AddWorldText(FString::FromInt(FMath::Abs(DeltaValue)), EWorldTextType::HealthRecover, DeltaValue < GetMaxHealth() ? EWorldTextStyle::Normal : EWorldTextStyle::Stress);
	}
}

void AAbilityCharacterBase::HandleMaxHealthChanged(float NewValue, float DeltaValue /*= 0.f*/)
{
	
}

void AAbilityCharacterBase::HandleMoveSpeedChanged(float NewValue, float DeltaValue /*= 0.f*/)
{
	GetCharacterMovement()->MaxWalkSpeed = NewValue * MovementRate;
}

void AAbilityCharacterBase::HandleRotationSpeedChanged(float NewValue, float DeltaValue)
{
	GetCharacterMovement()->RotationRate = FRotator(0, NewValue, 0) * RotationRate;
}

void AAbilityCharacterBase::HandleJumpForceChanged(float NewValue, float DeltaValue)
{
	GetCharacterMovement()->JumpZVelocity = NewValue;
}
