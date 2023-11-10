// Fill out your copyright notice in the Description page of Project Settings.


#include "Common/CommonStatics.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Asset/AssetModuleStatics.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "GameFramework/InputSettings.h"
#include "Gameplay/WHGameInstance.h"
#include "Gameplay/WHGameMode.h"
#include "Gameplay/WHGameState.h"
#include "Common/CommonTypes.h"
#include "Event/Handle/Common/EventHandle_PauseGame.h"
#include "Event/Handle/Common/EventHandle_UnPauseGame.h"
#include "Internationalization/Regex.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetStringLibrary.h"
#include "Main/MainModule.h"
#include "Main/MainModuleStatics.h"

bool UCommonStatics::IsPaused()
{
	return UGameplayStatics::IsGamePaused(GetWorldContext());
}

void UCommonStatics::SetPaused(bool bPaused)
{
	UGameplayStatics::SetGamePaused(GetWorldContext(), bPaused);
}

float UCommonStatics::GetTimeScale()
{
	return UGameplayStatics::GetGlobalTimeDilation(GetWorldContext());
}

float UCommonStatics::GetDeltaSeconds()
{
	return UGameplayStatics::GetWorldDeltaSeconds(GetWorldContext());
}

void UCommonStatics::SetTimeScale(float TimeScale)
{
	UGameplayStatics::SetGlobalTimeDilation(GetWorldContext(), TimeScale);
}

void UCommonStatics::PauseGame(EPauseMode PauseMode)
{
	switch(PauseMode)
	{
		case EPauseMode::Default:
		{
			SetPaused(true);
			break;
		}
		case EPauseMode::OnlyTime:
		{
			SetTimeScale(0.f);
			break;
		}
		case EPauseMode::OnlyModules:
		{
			UMainModuleStatics::PauseAllModule();
			break;
		}
	}
	UEventModuleStatics::BroadcastEvent<UEventHandle_PauseGame>(EEventNetType::Single, nullptr, { (int32)PauseMode } );
}

void UCommonStatics::UnPauseGame(EPauseMode PauseMode)
{
	switch(PauseMode)
	{
		case EPauseMode::Default:
		{
			SetPaused(false);
			break;
		}
		case EPauseMode::OnlyTime:
		{
			SetTimeScale(1.f);
			break;
		}
		case EPauseMode::OnlyModules:
		{
			UMainModuleStatics::UnPauseAllModule();
			break;
		}
	}
	UEventModuleStatics::BroadcastEvent<UEventHandle_UnPauseGame>(EEventNetType::Single, nullptr, { (int32)PauseMode } );
}

void UCommonStatics::QuitGame(TEnumAsByte<EQuitPreference::Type> QuitPreference, bool bIgnorePlatformRestrictions)
{
	UKismetSystemLibrary::QuitGame(GetWorldContext(), GetPlayerController<AWHPlayerController>(), QuitPreference, bIgnorePlatformRestrictions);
}

ETraceTypeQuery UCommonStatics::GetGameTraceType(ECollisionChannel InTraceChannel)
{
	return UEngineTypes::ConvertToTraceType(InTraceChannel);
}

bool UCommonStatics::IsInScreenViewport(const FVector& InWorldLocation)
{
	const APlayerController* PC = GetPlayerController();
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	if (LP && LP->ViewportClient)
	{
		// get the projection data
		FSceneViewProjectionData ProjectionData;
		if (LP->GetProjectionData(LP->ViewportClient->Viewport, ProjectionData))
		{
			FMatrix const ViewProjectionMatrix = ProjectionData.ComputeViewProjectionMatrix();
			FVector2D ScreenPosition;
			const bool bResult = FSceneView::ProjectWorldToScreen(InWorldLocation, ProjectionData.GetConstrainedViewRect(), ViewProjectionMatrix, ScreenPosition);
			if (bResult && ScreenPosition.X > ProjectionData.GetViewRect().Min.X && ScreenPosition.X < ProjectionData.GetViewRect().Max.X
				&& ScreenPosition.Y > ProjectionData.GetViewRect().Min.Y && ScreenPosition.Y < ProjectionData.GetViewRect().Max.Y)
			{
				return true;
			}
		}
	}
	return false;
}

int32 UCommonStatics::GetEnumItemNum(const FString& InEnumName)
{
	if(const UEnum* Enum = UAssetModuleStatics::FindObject<UEnum>(InEnumName, true))
	{
		return Enum->NumEnums() - 1;
	}
	return 0;
}

FString UCommonStatics::GetEnumValueAuthoredName(const FString& InEnumName, int32 InEnumValue)
{
	if(const UEnum* Enum = UAssetModuleStatics::FindEnumByValue(InEnumName, InEnumValue, true))
	{
		return Enum->GetAuthoredNameStringByValue(InEnumValue);
	}
	return TEXT("");
}

FText UCommonStatics::GetEnumValueDisplayName(const FString& InEnumName, int32 InEnumValue)
{
	if(const UEnum* Enum = UAssetModuleStatics::FindEnumByValue(InEnumName, InEnumValue, true))
	{
		return Enum->GetDisplayNameTextByValue(InEnumValue);
	}
	return FText::GetEmpty();
}

int32 UCommonStatics::GetEnumValueByAuthoredName(const FString& InEnumName, const FString& InEnumAuthoredName)
{
	if(const UEnum* Enum = UAssetModuleStatics::FindEnumByAuthoredName(InEnumName, InEnumAuthoredName))
	{
		return Enum->GetValueByNameString(InEnumAuthoredName);
	}
	return -1;
}

int32 UCommonStatics::GetEnumValueByDisplayName(const FString& InEnumName, const FString& InEnumDisplayName)
{
	if(const UEnum* Enum = UAssetModuleStatics::FindEnumByDisplayName(InEnumName, InEnumDisplayName))
	{
		for(int32 i = 0; i < Enum->NumEnums() - 1; i++)
		{
			if(Enum->GetDisplayNameTextByIndex(i).ToString().Equals(InEnumDisplayName))
			{
				return Enum->GetValueByIndex(i);
			}
		}
	}
	return -1;
}

void UCommonStatics::SaveObjectDataToMemory(UObject* InObject, TArray<uint8>& OutObjectData)
{
	if(InObject)
	{
		FMemoryWriter MemoryWriter(OutObjectData, true);
		FSaveDataArchive Ar(MemoryWriter);
		InObject->Serialize(Ar);
	}
}

void UCommonStatics::LoadObjectDataFromMemory(UObject* InObject, const TArray<uint8>& InObjectData)
{
	if(InObject && InObjectData.Num() > 0)
	{
		FMemoryReader MemoryReader(InObjectData, true);
		FSaveDataArchive Ar(MemoryReader);
		InObject->Serialize(Ar);
	}
}

void UCommonStatics::ImportExposedProperties(UObject* InObject, TSharedPtr<FJsonObject> InJsonObject)
{
    for(auto& Iter: InJsonObject->Values)
    {
        FString TempStr, PropertyName, PropertyType;
        Iter.Key.Split("> ",&TempStr,& PropertyName);
        TempStr.Split("<",&TempStr,&PropertyType);
        if(FProperty* Property = FindFProperty<FProperty>(InObject->GetClass(), * PropertyName))
        {
            if(Property->IsA<FObjectProperty>() && Iter.Value->Type == EJson::Object)
            {
                if(UObject* Object = CastField<FObjectProperty>(Property)->GetObjectPropertyValue(Property->ContainerPtrToValuePtr<UObject*>(InObject)))
                {
                    ImportExposedProperties(Object, Iter.Value->AsObject());
                }
            }
            else
            {
                void* Value = Property->ContainerPtrToValuePtr<void*>(InObject);
                Property->ImportText_Direct(*Iter.Value->AsString(), Value, InObject, PPF_None);
            }
        }
    }
}

void UCommonStatics::ExportExposedProperties(UObject* InObject, TSharedPtr<FJsonObject>& InJsonObject, bool bExportSubObjects)
{
	if(!InJsonObject) InJsonObject = MakeShared<FJsonObject>();
    const FString PropClassNameSuffix = "Property",ReplaceTo = "";
    for (TFieldIterator<FProperty> PropertyIt(InObject->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
    {
        FProperty* Property = *PropertyIt;
        const bool bFlag = Property->HasAllPropertyFlags(CPF_ExposeOnSpawn) || Property->HasAllPropertyFlags(CPF_BlueprintVisible);
        const bool bIsDelegate = Property->IsA<FDelegateProperty>() || Property->IsA<FMulticastDelegateProperty>();
        if(	bFlag && !bIsDelegate )
        {
            FString PropertyName = Property->GetFName().ToString();
            //添加属性类型
            PropertyName = "<" + Property->GetClass()->GetName().Replace(*PropClassNameSuffix, *ReplaceTo) + "> " + PropertyName;
            if(!InJsonObject->HasField(PropertyName))
            {
                if(Property->IsA<FObjectProperty>() && bExportSubObjects)
                {
                    if(UObject* Object = CastField<FObjectProperty>(Property)->GetObjectPropertyValue(Property->ContainerPtrToValuePtr<UObject*>(InObject)))
                    {
                        TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
                        ExportExposedProperties(Object, JsonObject);
                        InJsonObject->SetObjectField(PropertyName, JsonObject);
                    }
                }
                else
                {
                    FString Temp;
                    const void* Value = Property->ContainerPtrToValuePtr<void*>(InObject);
                    Property->ExportTextItem_Direct(Temp, Value, nullptr, nullptr, PPF_None);
                    InJsonObject->SetStringField(PropertyName,Temp);
                }
            }
        }
    }
}

void UCommonStatics::ExportPropertiesToObject(UObject* InSourceObject, UObject* InTargetObject, bool bExportSubObjects)
{
	TSharedPtr<FJsonObject> JsonObject;
	ExportExposedProperties(InSourceObject, JsonObject, bExportSubObjects);
	ImportExposedProperties(InTargetObject, JsonObject);
}

bool UCommonStatics::RegexMatch(const FString& InSourceStr, const FString& InPattern, TArray<FString>& OutResult)
{
	const FRegexPattern MatherPatter(InPattern);
	FRegexMatcher Matcher(MatherPatter, InSourceStr);
	while (Matcher.FindNext())
	{
		OutResult.Add(Matcher.GetCaptureGroup(0));
	}

	return OutResult.Num() == 0 ? false : true;
}

FString UCommonStatics::BoolToString(bool InBool)
{
	return InBool ? TEXT("true") : TEXT("false");
}

bool UCommonStatics::StringToBool(const FString& InString)
{
	return InString == TEXT("true");
}

TArray<FString> UCommonStatics::NotNumberSymbols = TArray<FString>{ TEXT("."), TEXT(","), TEXT(" ") };

bool UCommonStatics::TextIsNumber(const FText& InText)
{
	const FString TextStr = InText.ToString();
	for(auto& Iter : NotNumberSymbols)
	{
		if(TextStr.Contains(Iter))
		{
			return false;
		}
	}
	return true;
}

int32 UCommonStatics::TextToNumber(const FText& InText, TMap<int32, FString>& OutSymbols)
{
	FString TextStr = InText.ToString();
	TArray<FString> TextArr = UKismetStringLibrary::GetCharacterArrayFromString(TextStr);
	OutSymbols.Empty();
	for(int32 i = TextArr.Num() - 1; i >= 0; i--)
	{
		if(NotNumberSymbols.Contains(TextArr[i]))
		{
			OutSymbols.Add(TextArr.Num() - i - 1, TextArr[i]);
		}
	}
	for(auto& Iter : NotNumberSymbols)
	{
		TextStr = TextStr.Replace(*Iter, *FString(""));
	}
	return FCString::Atoi(*TextStr);
}

FText UCommonStatics::NumberToText(int32 InNumber, const TMap<int32, FString>& InSymbols)
{
	FString TextStr = FString::FromInt(InNumber);
	for(auto& Iter : InSymbols)
	{
		const int32 InsertIndex = TextStr.Len() - Iter.Key;
		if(InsertIndex > 0)
		{
			TextStr.InsertAt(InsertIndex, Iter.Value);
		}
	}
	return FText::FromString(TextStr);
}

bool UCommonStatics::ParseJsonObjectFromString(const FString& InJsonString, TSharedPtr<FJsonObject>& OutJsonObject)
{
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(InJsonString);
    return FJsonSerializer::Deserialize(JsonReader, OutJsonObject);
}

FText UCommonStatics::GetInputActionKeyCodeByName(const FString& InInputActionName)
{
	TArray<FInputActionKeyMapping> KeyMappings;
	UInputSettings::GetInputSettings()->GetActionMappingByName(*InInputActionName, KeyMappings);
	for(const auto& Iter : KeyMappings)
	{
		return FText::FromString(Iter.Key.GetFName().ToString());
	}
	return FText::GetEmpty();
}

bool UCommonStatics::ExecuteObjectFunc(UObject* InObject, const FName& InFuncName, void* Params)
{
	if (ensureEditor(InObject))
	{
		UFunction* Func = InObject->FindFunction(InFuncName);
		if (ensureEditorMsgf(Func, FString::Printf(TEXT("错误的函数调用, %s 对应函数名称 : %s ,并不存在"), *InObject->GetName(), *InFuncName.ToString()), EDC_Default, EDV_Error))
		{
			InObject->ProcessEvent(Func, Params);
			return true;
		}
	}
	return false;
}

TArray<UClass*> UCommonStatics::GetClassChildren(const UClass* InClass, bool bIncludeSelf)
{
	TArray<UClass*> ReturnValues;
	for (TObjectIterator<UClass> Iter; Iter; ++Iter)
	{
		UClass* Class = *Iter;
		if (Class->IsChildOf(InClass) && (bIncludeSelf || Class != InClass))
		{
			ReturnValues.Add(Class);
		}
	}
	return ReturnValues;
}

UTexture2D* UCommonStatics::LoadTextureFormFile(const FString& InFilePath)
{
	UTexture2D* Texture = nullptr;
	TArray<uint8> CompressedData;
	if (FFileHelper::LoadFileToArray(CompressedData, *InFilePath))
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		TSharedPtr<IImageWrapper> ImageWrappers[4] =
		{
			ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG),
			ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG),
			ImageWrapperModule.CreateImageWrapper(EImageFormat::BMP),
			ImageWrapperModule.CreateImageWrapper(EImageFormat::EXR)
		};
		for ( auto ImageWrapper : ImageWrappers )
		{
			if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(CompressedData.GetData(), CompressedData.Num()))
			{
				TArray<uint8> RawData;
				if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
				{
					Texture = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_B8G8R8A8);
					if (Texture)
					{
						Texture->AddToRoot();
						const auto PlatformData = new FTexturePlatformData();
						Texture->SetPlatformData(PlatformData);
						void* TextureData = PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
						FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
						PlatformData->Mips[0].BulkData.Unlock();
						Texture->UpdateResource();
					}
				}
				break;
			}
		}
	}
	return Texture;
}

void UCommonStatics::SaveTextureToFile(UTexture2D* InTexture, const FString& InFilePath)
{
	if(!InTexture) return;

	if(FTexturePlatformData* PlatformData = InTexture->GetPlatformData())
	{
		FTexture2DMipMap& MipMap = PlatformData->Mips[0];
		const unsigned char* Data = (unsigned char*)MipMap.BulkData.Lock(LOCK_READ_WRITE);
		const int32 TexSizeX = MipMap.SizeX;
		const int32 TexSizeY = MipMap.SizeY;
		TArray<FColor> Colors;
		Colors.SetNum(TexSizeX * TexSizeY);

		FMemory::Memcpy(Colors.GetData(), Data, (int32)(sizeof(FColor)) * TexSizeX * TexSizeY);
		MipMap.BulkData.Unlock();

		TArray<uint8> ImgData;
		FImageUtils::ThumbnailCompressImageArray(TexSizeX, TexSizeY, Colors, ImgData);

		FFileHelper::SaveArrayToFile(ImgData, *InFilePath);
	}
}

UTexture2D* UCommonStatics::CompositeTextures(const TArray<UTexture2D*>& InTextures, FVector2D InTexSize, UTexture2D* InTemplate)
{
	if(InTextures.Num() == 0) return nullptr;

    // 获取模板纹理
    UTexture2D* TemplateTexture = InTemplate ? InTemplate : InTextures[0];

    // 得到模板纹理的平台数据
    FTexturePlatformData* TemplatePlatformData = TemplateTexture->GetPlatformData();

	const int32 TemplateSizeX = TemplatePlatformData->SizeX;
	const int32 TemplateSizeY = TemplatePlatformData->SizeY;

	// 取得模板纹理的像素格式
	const EPixelFormat PixelFormat = TemplatePlatformData->PixelFormat;
	const FPixelFormatInfo& PixelFormatInfo = GPixelFormats[PixelFormat];

    // 取得模板纹理的长宽
    const int32 SizeX = InTexSize.X;
    const int32 SizeY = InTexSize.Y;

	if(ensureEditorMsgf(SizeX > 0 && SizeY > 0 &&
		(SizeX % PixelFormatInfo.BlockSizeX) == 0 &&
		(SizeY % PixelFormatInfo.BlockSizeY) == 0, TEXT("Invalid size and/or pixel format for new texture"), EDC_Default, EDV_Error))
	{
		UTexture2D* Texture = NewObject<UTexture2D>(GetTransientPackage(), NAME_None, RF_Transient );

		const int32 NumSlicesX = SizeX / TemplateSizeX;
		const int32 NumSlicesY = SizeY / TemplateSizeY;

		// 设置一般属性
		Texture->AddressX = TemplateTexture->AddressX;
		Texture->AddressY = TemplateTexture->AddressY;
		Texture->SRGB = TemplateTexture->SRGB;
		Texture->Filter = TemplateTexture->Filter;
		Texture->LODGroup = TemplateTexture->LODGroup;
		// Texture->MipGenSettings = TemplateTexture->MipGenSettings;
		Texture->MipLoadOptions = TemplateTexture->MipLoadOptions;
		Texture->CompressionSettings = TemplateTexture->CompressionSettings;

		const int32 NumBlockBytes = PixelFormatInfo.BlockBytes;

		const int32 NumBlocksX = SizeX / PixelFormatInfo.BlockSizeX;
		const int32 NumBlocksY = SizeY / PixelFormatInfo.BlockSizeY;

		const int32 TemplateNumBlocksX = TemplateSizeX / PixelFormatInfo.BlockSizeX;
		const int32 TemplateNumBlocksY = TemplateSizeY / PixelFormatInfo.BlockSizeY;

		TArray<UTexture2D*> SliceTextures;
		// 筛选有效的纹理
		for (auto Item : InTextures)
		{
			const FTexturePlatformData* SourcePlatformData = Item->GetPlatformData();

			bool bIsInvalid = false;

			// 只有长宽与纹理格式都符号模板才有效
			bIsInvalid |= SourcePlatformData->SizeX != TemplatePlatformData->SizeX;
			bIsInvalid |= SourcePlatformData->SizeY != TemplatePlatformData->SizeY;
			bIsInvalid |= SourcePlatformData->PixelFormat != PixelFormat;
			bIsInvalid |= SourcePlatformData->Mips.Num() != TemplatePlatformData->Mips.Num();

			if (!bIsInvalid)
			{
				SliceTextures.AddUnique(Item);
			}
		}

		// 纹理切片数量
		const int32 NumSlices = SliceTextures.Num();

		// 获取纹理数组的平台数据
		FTexturePlatformData* PlatformData = new FTexturePlatformData();
		Texture->SetPlatformData(PlatformData);

		// 设置纹理长宽元素数与像素格式
		PlatformData->SizeX = SizeX;
		PlatformData->SizeY = SizeY;
		PlatformData->SetNumSlices(1);
		PlatformData->PixelFormat = PixelFormat;

		// 遍历每个Mip层
		for (int32 MipIndex = 0; MipIndex < TemplatePlatformData->Mips.Num(); ++MipIndex)
		{
			// 取得纹理数组中的Mip层对象
			FTexture2DMipMap* Mip = new FTexture2DMipMap();
			PlatformData->Mips.Add(Mip);

			// 设置当前Mip层长宽及元素数
			Mip->SizeX = SizeX;
			Mip->SizeY = SizeY;
			Mip->SizeZ = 1;

			// 以读写方式锁定当前Mip层
			Mip->BulkData.Lock(LOCK_READ_WRITE);

			// 重置Mip层到所需大小
			void* BulkData = Mip->BulkData.Realloc((int64)NumBlocksX * NumBlocksY * NumBlockBytes);

			// 遍历每个纹理元素
			for (int32 SliceIndex = 0; SliceIndex < NumSlicesX * NumSlicesY; ++SliceIndex)
			{
				// 获取源纹理平台数据
				FTexturePlatformData* SourcePlatformData = SliceIndex < NumSlices ? SliceTextures[SliceIndex]->GetPlatformData() : TemplatePlatformData;
        
				// 获取源纹理对应Mip层
				FTexture2DMipMap& SourceMip = SourcePlatformData->Mips[MipIndex];
        
				// 以只读方式锁定源纹理Mip层
				const void* SourceBulkData = SourceMip.BulkData.Lock(LOCK_READ_ONLY);
        
				// 从源纹理复制数据到纹理数组中
				if (NumSlicesX > 1)
				{
					for(int32 SourceByteYIndex = 0; SourceByteYIndex < TemplateNumBlocksY * NumBlockBytes; SourceByteYIndex++)
					{
						FMemory::Memcpy((uint8*)BulkData + (SliceIndex / NumSlicesX * NumBlocksX * TemplateNumBlocksY * NumBlockBytes +
							SliceIndex % NumSlicesX * TemplateNumBlocksX * NumBlockBytes + NumBlocksX * SourceByteYIndex * NumBlockBytes),
							(uint8*)SourceBulkData + SourceByteYIndex * TemplateNumBlocksX * NumBlockBytes, TemplateNumBlocksX * NumBlockBytes);
					}
				}
				else
				{
					FMemory::Memcpy((uint8*)BulkData + SliceIndex * TemplateNumBlocksX * TemplateNumBlocksY * NumBlockBytes,
						SourceBulkData, TemplateNumBlocksX * TemplateNumBlocksY * NumBlockBytes);
				}
        
				// 解锁源纹理Mip层
				SourceMip.BulkData.Unlock();
			}

			// 解锁当前Mip层
			Mip->BulkData.Unlock();
		}

		// 更新纹理数组渲染资源
		Texture->UpdateResource();
	
		return Texture;
	}
	return nullptr;
}

bool UCommonStatics::IsImplementedInBlueprint(const UFunction* Func)
{
	return Func && ensure(Func->GetOuter()) && Func->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
}

UObject* UCommonStatics::GetWorldContext(bool bInEditor)
{
	return AMainModule::Get(bInEditor);
}

UWHGameInstance* UCommonStatics::GetGameInstance(TSubclassOf<UWHGameInstance> InClass)
{
	return Cast<UWHGameInstance>(UGameplayStatics::GetGameInstance(GetWorldContext()));
}

AWHGameMode* UCommonStatics::GetGameMode(TSubclassOf<AWHGameMode> InClass)
{
	return Cast<AWHGameMode>(UGameplayStatics::GetGameMode(GetWorldContext()));
}

AWHGameState* UCommonStatics::GetGameState(TSubclassOf<AWHGameState> InClass)
{
	return Cast<AWHGameState>(UGameplayStatics::GetGameState(GetWorldContext()));
}

AWHPlayerController* UCommonStatics::GetPlayerController(TSubclassOf<AWHPlayerController> InClass, int32 InPlayerIndex)
{
	return Cast<AWHPlayerController>(UGameplayStatics::GetPlayerController(GetWorldContext(), InPlayerIndex));
}

AWHPlayerController* UCommonStatics::GetPlayerControllerByID(TSubclassOf<AWHPlayerController> InClass, int32 InPlayerID)
{
	return Cast<AWHPlayerController>(UGameplayStatics::GetPlayerControllerFromID(GetWorldContext(), InPlayerID));
}

AWHPlayerController* UCommonStatics::GetLocalPlayerController(TSubclassOf<AWHPlayerController> InClass)
{
	for(auto Iter = GetCurrentWorld()->GetPlayerControllerIterator(); Iter; ++Iter)
	{
		AWHPlayerController* PlayerController = Cast<AWHPlayerController>(Iter->Get());
		if(PlayerController && PlayerController->IsLocalController())
		{
			return PlayerController;
		}
	}
	return nullptr;
}

APawn* UCommonStatics::GetPossessedPawn(TSubclassOf<APawn> InClass, int32 InPlayerIndex)
{
	if(AWHPlayerController* PlayerController = GetPlayerController<AWHPlayerController>(InPlayerIndex))
	{
		return PlayerController->GetPawn();
	}
	return nullptr;
}

APawn* UCommonStatics::GetPossessedPawnByID(int32 InPlayerID, TSubclassOf<APawn> InClass)
{
	if(AWHPlayerController* PlayerController = GetPlayerControllerByID<AWHPlayerController>(InPlayerID))
	{
		return PlayerController->GetPawn();
	}
	return nullptr;
}

APawn* UCommonStatics::GetLocalPossessedPawn(TSubclassOf<APawn> InClass)
{
	if(AWHPlayerController* PlayerController = GetLocalPlayerController<AWHPlayerController>())
	{
		return PlayerController->GetPawn();
	}
	return nullptr;
}

APawn* UCommonStatics::GetPlayerPawn(TSubclassOf<APawn> InClass, int32 InPlayerIndex)
{
	if(AWHPlayerController* PlayerController = GetPlayerController<AWHPlayerController>(InPlayerIndex))
	{
		return PlayerController->GetPlayerPawn();
	}
	return nullptr;
}

APawn* UCommonStatics::GetPlayerPawnByID(int32 InPlayerID, TSubclassOf<APawn> InClass)
{
	if(AWHPlayerController* PlayerController = GetPlayerControllerByID<AWHPlayerController>(InPlayerID))
	{
		return PlayerController->GetPlayerPawn();
	}
	return nullptr;
}

APawn* UCommonStatics::GetLocalPlayerPawn(TSubclassOf<APawn> InClass)
{
	if(AWHPlayerController* PlayerController = GetLocalPlayerController<AWHPlayerController>())
	{
		return PlayerController->GetPlayerPawn();
	}
	return nullptr;
}

TArray<ULocalPlayer*> UCommonStatics::GetLocalPlayers()
{
	return GetGameInstance()->GetLocalPlayers();
}

ULocalPlayer* UCommonStatics::GetLocalPlayer(int32 InPlayerID, TSubclassOf<ULocalPlayer> InClass)
{
	if(GetLocalPlayers().IsValidIndex(InPlayerID))
	{
		return GetLocalPlayers()[InPlayerID];
	}
	return nullptr;
}
