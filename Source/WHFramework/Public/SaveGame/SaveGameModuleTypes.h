
#pragma once

#include "CoreMinimal.h"

#include "SaveGameModuleTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FSaveData()
	{
		bSaved = false;
		Datas = TArray<uint8>();
	}

	virtual ~FSaveData() { }

protected:
	UPROPERTY(Transient)
	bool bSaved;

	UPROPERTY(BlueprintReadWrite)
	TArray<uint8> Datas;

public:
	virtual bool IsValid() const { return true; }

	virtual bool IsSaved() const { return bSaved; }

	virtual void MakeSaved() { bSaved = true; }

	virtual TArray<uint8>& GetDatas() { return Datas; }

	virtual const TArray<uint8>& GetDatas() const { return Datas; }

	virtual void SetDatas(const TArray<uint8>& InDatas) { Datas = InDatas; }

public:
	template<class T>
	T* Cast()
	{
		return static_cast<T*>(this);
	}

	template<class T>
	T& CastRef()
	{
		return *Cast<T>();
	}
};

UENUM(BlueprintType)
enum class ESaveScope : uint8
{
	None,
	Profile,
	World
};

/** 每个关卡独立选择：临时会话不会打开或修改游戏存档与设置 Profile。 */
UENUM(BlueprintType)
enum class ESaveSessionPolicy : uint8
{
	Persistent,
	Transient
};

