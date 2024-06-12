#include "WHFrameworkCoreStatics.h"

#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "WHFrameworkCoreTypes.h"
#include "Engine/Texture.h"

void FCoreStatics::SaveObjectDataToMemory(UObject* InObject, TArray<uint8>& OutObjectData)
{
	if(InObject)
	{
		FMemoryWriter MemoryWriter(OutObjectData, true);
		FSaveDataArchive Ar(MemoryWriter);
		InObject->Serialize(Ar);
	}
}

void FCoreStatics::LoadObjectDataFromMemory(UObject* InObject, const TArray<uint8>& InObjectData)
{
	if(InObject && InObjectData.Num() > 0)
	{
		FMemoryReader MemoryReader(InObjectData, true);
		FSaveDataArchive Ar(MemoryReader);
		InObject->Serialize(Ar);
	}
}

void FCoreStatics::ImportExposedProperties(UObject* InObject, TSharedPtr<FJsonObject> InJsonObject)
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

void FCoreStatics::ExportExposedProperties(UObject* InObject, TSharedPtr<FJsonObject>& InJsonObject, bool bExportSubObjects)
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

void FCoreStatics::ExportPropertiesToObject(UObject* InSourceObject, UObject* InTargetObject, bool bExportSubObjects)
{
	TSharedPtr<FJsonObject> JsonObject;
	ExportExposedProperties(InSourceObject, JsonObject, bExportSubObjects);
	ImportExposedProperties(InTargetObject, JsonObject);
}

bool FCoreStatics::RegexMatch(const FString& InSourceStr, const FString& InPattern, TArray<FString>& OutResult)
{
	const FRegexPattern MatherPatter(InPattern);
	FRegexMatcher Matcher(MatherPatter, InSourceStr);
	while (Matcher.FindNext())
	{
		OutResult.Add(Matcher.GetCaptureGroup(0));
	}

	return OutResult.Num() == 0 ? false : true;
}

UTexture2D* FCoreStatics::LoadTextureFromFile(const FString& InFilePath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if(!PlatformFile.FileExists(*InFilePath)) return nullptr;
	
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
						const auto PlatformData = Texture->GetPlatformData();
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

void FCoreStatics::SaveTextureToFile(UTexture2D* InTexture, const FString& InFilePath)
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

UTexture2D* FCoreStatics::CompositeTextures(const TArray<UTexture2D*>& InTextures, FVector2D InTexSize, UTexture2D* InTemplate)
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

	if(ensure(SizeX > 0 && SizeY > 0 &&
		(SizeX % PixelFormatInfo.BlockSizeX) == 0 &&
		(SizeY % PixelFormatInfo.BlockSizeY) == 0))
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
