/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
*/

#include "HoudiniCSVFactory.h"
#include "HoudiniCSV.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "HoudiniCSVFactory"
 
/////////////////////////////////////////////////////
// UHoudiniCSVFactory
 
UHoudiniCSVFactory::UHoudiniCSVFactory(const FObjectInitializer& ObjectInitializer) : Super( ObjectInitializer )
{
    SupportedClass = UHoudiniCSV::StaticClass();
    //bCreateNew = true;

    // for the new menu
    bEditAfterNew = true;

    bText = true;

    // for the import menu
    Formats.Add(FString(TEXT("csv;")) + NSLOCTEXT("HoudiniCSVFactory", "FormatCSV", "CSV File").ToString());
    Formats.Add(FString(TEXT("hcsv;")) + NSLOCTEXT("HoudiniCSVFactory", "FormatHCSV", "HCSV File").ToString());
    bEditorImport = true;
}


UObject* UHoudiniCSVFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
    UHoudiniCSV* NewHoudiniCSVObject = NewObject<UHoudiniCSV>(InParent, Class, Name, Flags | RF_Transactional);
    return NewHoudiniCSVObject;
}

/*
UObject* UHoudiniCSVFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
    UHoudiniCSV* NewHoudiniCSVObject = NewObject<UHoudiniCSV>( InParent, InClass, InName, Flags | RF_Transactional );
    return NewHoudiniCSVObject;
}
*/

UObject* UHoudiniCSVFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
    bOutOperationCanceled = false;

    UHoudiniCSV* NewHoudiniCSVObject = NewObject<UHoudiniCSV>(InParent, InClass, InName, Flags);
    if ( !NewHoudiniCSVObject )
	return nullptr;

    if ( !NewHoudiniCSVObject->UpdateFromFile( Filename ) )
	return nullptr;    

    return NewHoudiniCSVObject;
}

FText UHoudiniCSVFactory::GetDisplayName() const
{
    return LOCTEXT("HoudiniCSVFactoryDescription", "Houdini CSV File");
}

bool UHoudiniCSVFactory::DoesSupportClass(UClass * Class)
{
    return ( Class == UHoudiniCSV::StaticClass() /*|| Class == UDataTable::StaticClass()*/);
}

bool UHoudiniCSVFactory::FactoryCanImport(const FString& Filename)
{
    const FString Extension = FPaths::GetExtension(Filename);

    if ( Extension == TEXT("csv") || Extension == TEXT("hcsv") )
    {
	return true;
    }
    return false;
}

#undef LOCTEXT_NAMESPACE