// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "AudioModuleTypes.generated.h"

class USoundBase;

DECLARE_DYNAMIC_DELEGATE(FOnSoundPlayFinishDelegate);

USTRUCT(BlueprintType)
struct FSingleSoundHandle
{
	GENERATED_USTRUCT_BODY()

	FSingleSoundHandle()
		: Handle(INDEX_NONE)
	{
	}

	FSingleSoundHandle(int32 InHandle)
		: Handle(InHandle)
	{
	}

	bool IsValid() const
	{
		return Handle != INDEX_NONE;
	}

	bool operator==(const FSingleSoundHandle& Other) const
	{
		return Handle == Other.Handle;
	}

	bool operator!=(const FSingleSoundHandle& Other) const
	{
		return Handle != Other.Handle;
	}

	friend uint32 GetTypeHash(const FSingleSoundHandle& SpecHandle)
	{
		return ::GetTypeHash(SpecHandle.Handle);
	}

	FString ToString() const
	{
		return IsValid() ? FString::FromInt(Handle) : TEXT("Invalid");
	}

private:
	UPROPERTY()
	int32 Handle;
};

USTRUCT(BlueprintType)
struct FSingleSoundInfo
{
	GENERATED_USTRUCT_BODY()

	FSingleSoundInfo()
	{
		Sound = nullptr;
		Audio = nullptr;
	}

	FSingleSoundInfo(UAudioComponent* InAudio, USoundBase* InSound)
	{
		Sound = InSound;
		Audio = InAudio;
	}

	bool IsValid() const
	{
		return Audio != nullptr;
	}

public:
	UPROPERTY(BlueprintReadOnly)
	USoundBase* Sound;

	UPROPERTY(BlueprintReadOnly)
	UAudioComponent* Audio;
};