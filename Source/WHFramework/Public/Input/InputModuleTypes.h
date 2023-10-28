// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "GameFramework/PlayerController.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "EnhancedActionKeyMapping.h"
#include "CommonInputTypeEnum.h"
#include "GameplayTagContainer.h"

#include "InputModuleTypes.generated.h"

/**
* 输入模式
*/
UENUM(BlueprintType)
enum class EInputMode : uint8
{
	/// 无
	None,
	/// 游戏模式
	GameOnly,
	/// UI模式
	UIOnly,
	/// 游戏和UI模式
	GameAndUI,
	/// 游戏和UI模式_不隐藏光标
	GameAndUI_NotHideCursor
};

UENUM(BlueprintType)
enum class EInputInteractAction : uint8
{
	Action1,
	Action2,
	Action3
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputKeyShortcut
{
	GENERATED_BODY()

public:
	FInputKeyShortcut()
	{
		Key = FKey();
		Auxs = TArray<FKey>();
	}

	FInputKeyShortcut(const FKey& InKey)
		: Key(InKey),
		  Auxs({})
	{
	}

	FInputKeyShortcut(const FKey& InKey, const TArray<FKey>& InAuxs)
		: Key(InKey),
		  Auxs(InAuxs)
	{
	}

public:
	bool IsValid() const
	{
		return Key.IsValid();
	}
	
	bool IsPressing(APlayerController* InPlayerController, bool bAllowInvalid = false) const;

public:
	UPROPERTY(EditAnywhere)
	FKey Key;

	UPROPERTY(EditAnywhere)
	TArray<FKey> Auxs;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputKeyMapping
{
	GENERATED_BODY()

public:
	FInputKeyMapping()
	{
		Key = FKey();
		Event = EInputEvent::IE_MAX;
	}

	FInputKeyMapping(const FKey& InKey, EInputEvent InEvent, const FInputActionHandlerWithKeySignature& InDelegate)
		: Key(InKey),
		  Event(InEvent),
		  Delegate(InDelegate)
	{
	}
	
public:
	UPROPERTY(BlueprintReadWrite)
	FKey Key;

	UPROPERTY(BlueprintReadWrite)
	TEnumAsByte<EInputEvent> Event;

	FInputActionHandlerWithKeySignature Delegate;

	UPROPERTY(BlueprintReadWrite)
	FInputActionHandlerDynamicSignature DynamicDelegate;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputTouchMapping
{
	GENERATED_BODY()

public:
	FInputTouchMapping()
	{
		Event = EInputEvent::IE_MAX;
	}

	FInputTouchMapping(EInputEvent InEvent, const FInputTouchHandlerSignature& InDelegate)
		: Event(InEvent),
		  Delegate(InDelegate)
	{
	}

public:
	UPROPERTY(BlueprintReadWrite)
	TEnumAsByte<EInputEvent> Event;

	FInputTouchHandlerSignature Delegate;

	UPROPERTY(BlueprintReadWrite)
	FInputTouchHandlerDynamicSignature DynamicDelegate;
};

struct WHFRAMEWORK_API FInputModeNone : public FInputModeDataBase
{
	FInputModeNone()
	{}

protected:
	virtual void ApplyInputMode(class FReply& SlateOperations, class UGameViewportClient& GameViewportClient) const override;
};

struct WHFRAMEWORK_API FInputModeGameAndUI_NotHideCursor : public FInputModeGameAndUI
{
	FInputModeGameAndUI_NotHideCursor()
	{
		bHideCursorDuringCapture = false;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputActionMappings : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FInputActionMappings()
	{
		Mappings = TArray<FEnhancedActionKeyMapping>();
	}

	FORCEINLINE FInputActionMappings(TArray<FEnhancedActionKeyMapping> InMappings)
	{
		Mappings = InMappings;
	}

public:
	UPROPERTY()
	TArray<FEnhancedActionKeyMapping> Mappings;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FInputModuleSaveData()
	{
		ActionMappings = TArray<FInputActionMappings>();
		KeyShortcuts = TMap<FName, FKey>();
		KeyMappings = TMap<FName, FKey>();
	}

public:
	UPROPERTY()
	TArray<FInputActionMappings> ActionMappings;

	UPROPERTY()
	TMap<FName, FKey> KeyShortcuts;

	UPROPERTY()
	TMap<FName, FKey> KeyMappings;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();

		for(auto& Iter : ActionMappings)
		{
			Iter.MakeSaved();
		}
	}

	virtual TArray<FEnhancedActionKeyMapping*> GetActionMappingsByName(const FName InActionName);
};

class UPlayerMappableInputConfig;

/** A container to organize loaded player mappable configs to their CommonUI input type */
USTRUCT(BlueprintType)
struct FLoadedInputConfigMapping
{
	GENERATED_BODY()

	FLoadedInputConfigMapping() = default;
	FLoadedInputConfigMapping(const UPlayerMappableInputConfig* InConfig, ECommonInputType InType, const bool InIsActive)
		: Config(InConfig)
		, Type(InType)
		, bIsActive(InIsActive)
	{}

	/** The player mappable input config that should be applied to the Enhanced Input subsystem */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TObjectPtr<const UPlayerMappableInputConfig> Config = nullptr;

	/** The type of device that this mapping config should be applied to */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	ECommonInputType Type = ECommonInputType::Count;

	/** If this config is currently active. A config is marked as active when it's owning GFA is active */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bIsActive = false;
};

/** A container to organize potentially unloaded player mappable configs to their CommonUI input type */
USTRUCT()
struct FInputConfigMapping
{
	GENERATED_BODY()
	
	FInputConfigMapping() = default;
	
	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UPlayerMappableInputConfig> Config;

	/**
	 * The type of config that this is. Useful for filtering out configs by the current input device
	 * for things like the settings screen, or if you only want to apply this config when a certain
	 * input type is being used.
	 */
	UPROPERTY(EditAnywhere)
	ECommonInputType Type = ECommonInputType::Count;

	/**
	 * Container of platform traits that must be set in order for this input to be activated.
	 * 
	 * If the platform does not have one of the traits specified it can still be registered, but cannot
	 * be activated. 
	 */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer DependentPlatformTraits;

	/**
	 * If the current platform has any of these traits, then this config will not be actived.
	 */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer ExcludedPlatformTraits;

	/** If true, then this input config will be activated when it's associated Game Feature is activated.
	 * This is normally the desirable behavior
	 */
	UPROPERTY(EditAnywhere)
	bool bShouldActivateAutomatically = true;

	/** Returns true if this config pair can be activated based on the current platform traits and settings. */
	bool CanBeActivated() const;
	
	/**
	 * Registers the given config mapping with the local settings
	 */
	static bool RegisterPair(const FInputConfigMapping& Pair);

	/**
	 * Unregisters the given config mapping with the local settings
	 */
	static void UnregisterPair(const FInputConfigMapping& Pair);
};
