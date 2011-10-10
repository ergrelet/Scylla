#include "ImportRebuild.h"

#include "Logger.h"
#include "ConfigurationHolder.h"

#define DEBUG_COMMENTS


bool ImportRebuild::splitTargetFile()
{
	PIMAGE_SECTION_HEADER pSecHeader = 0;
	WORD i = 0;
	BYTE * data = 0;
	DWORD alignment = 0;
	DWORD dwSize = 0;

	pDosHeader = new IMAGE_DOS_HEADER;
	CopyMemory(pDosHeader, imageData, sizeof(IMAGE_DOS_HEADER));

	if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
	{
		return false;
	}

	pNTHeader = new IMAGE_NT_HEADERS;
	CopyMemory(pNTHeader, (PVOID)((DWORD_PTR)imageData + pDosHeader->e_lfanew), sizeof(IMAGE_NT_HEADERS));

	if (pNTHeader->Signature != IMAGE_NT_SIGNATURE)
	{
		return false;
	}

	if (pDosHeader->e_lfanew > sizeof(IMAGE_DOS_HEADER))
	{
		dwSize = pDosHeader->e_lfanew - sizeof(IMAGE_DOS_HEADER);
		pDosStub = new BYTE[dwSize];
		CopyMemory(pDosStub, (PVOID)((DWORD_PTR)imageData + sizeof(IMAGE_DOS_HEADER)), dwSize);
	}
	else
	{
		pDosStub = 0;
	}

	pSecHeader = IMAGE_FIRST_SECTION((PIMAGE_NT_HEADERS)((DWORD_PTR)imageData + pDosHeader->e_lfanew));

	for (i = 0; i < pNTHeader->FileHeader.NumberOfSections; i++)
	{
		dwSize = pSecHeader->SizeOfRawData;

		if (dwSize > 300000000)
		{
			dwSize = 300000000;
		}

		//TODO better use section alignment because it is better?
		alignment = alignValue(dwSize, pNTHeader->OptionalHeader.SectionAlignment);
		data = new BYTE[alignment];

		ZeroMemory(data, alignment);
		CopyMemory(data, (PVOID)((DWORD_PTR)imageData + pSecHeader->PointerToRawData), dwSize);

		vecSectionData.push_back(data);
		vecSectionHeaders.push_back(*pSecHeader);

		pSecHeader++;
	}

	delete [] imageData;
	imageData = 0;

	return true;
}

bool ImportRebuild::alignSectionHeaders()
{
	for (WORD i = 0; i < vecSectionHeaders.size(); i++)
	{
		vecSectionHeaders[i].VirtualAddress = alignValue(vecSectionHeaders[i].VirtualAddress, pNTHeader->OptionalHeader.SectionAlignment);
		vecSectionHeaders[i].Misc.VirtualSize = alignValue(vecSectionHeaders[i].Misc.VirtualSize, pNTHeader->OptionalHeader.SectionAlignment);

		vecSectionHeaders[i].PointerToRawData = alignValue(vecSectionHeaders[i].PointerToRawData, pNTHeader->OptionalHeader.FileAlignment);
		vecSectionHeaders[i].SizeOfRawData = alignValue(vecSectionHeaders[i].SizeOfRawData, pNTHeader->OptionalHeader.FileAlignment);
	}

	return true;
}

bool ImportRebuild::saveNewFile(const WCHAR * filepath)
{
	DWORD fileOffset = 0;
	DWORD dwWriteSize = 0;
	size_t i = 0;

	if (vecSectionHeaders.size() != vecSectionData.size())
	{
		return false;
	}

	HANDLE hFile = CreateFile(filepath, GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0,CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

	if(hFile == INVALID_HANDLE_VALUE)
	{
#ifdef DEBUG_COMMENTS
		Logger::debugLog("saveNewFile :: INVALID_HANDLE_VALUE %u\r\n",GetLastError());
#endif

		return false;
	}

	//alignSectionHeaders();
	updatePeHeader();

	fileOffset = 0;
	dwWriteSize = sizeof(IMAGE_DOS_HEADER);
	ProcessAccessHelp::writeMemoryToFile(hFile, fileOffset, dwWriteSize, pDosHeader);

	fileOffset += dwWriteSize;
	dwWriteSize = pDosHeader->e_lfanew - sizeof(IMAGE_DOS_HEADER);
	ProcessAccessHelp::writeMemoryToFile(hFile, fileOffset, dwWriteSize, pDosStub);

	fileOffset += dwWriteSize;
	dwWriteSize = sizeof(IMAGE_NT_HEADERS);
	ProcessAccessHelp::writeMemoryToFile(hFile, fileOffset, dwWriteSize, pNTHeader);

	fileOffset += dwWriteSize;
	dwWriteSize = sizeof(IMAGE_SECTION_HEADER);

	for (i = 0; i < vecSectionHeaders.size(); i++)
	{
		if (!ProcessAccessHelp::writeMemoryToFile(hFile, fileOffset, dwWriteSize, &vecSectionHeaders[i]))
		{
#ifdef DEBUG_COMMENTS
			Logger::debugLog(TEXT("saveNewFile :: writeMemoryToFile failed offset %X size %X\r\n"),fileOffset,dwWriteSize);
#endif
			CloseHandle(hFile);
			return false;
		}
		fileOffset += dwWriteSize;
	}

	for (i = 0; i < vecSectionHeaders.size(); i++)
	{
		dwWriteSize = vecSectionHeaders[i].PointerToRawData - fileOffset;

		if (dwWriteSize)
		{
			if (!writeZeroMemoryToFile(hFile, fileOffset, dwWriteSize))
			{
#ifdef DEBUG_COMMENTS
				Logger::debugLog(TEXT("saveNewFile :: writeZeroMemoryToFile failed offset %X size %X\r\n"),fileOffset,dwWriteSize);
#endif
				CloseHandle(hFile);
				return false;
			}
			fileOffset += dwWriteSize;
		}

		dwWriteSize = vecSectionHeaders[i].SizeOfRawData;

		ProcessAccessHelp::writeMemoryToFile(hFile, fileOffset, dwWriteSize, vecSectionData[i]);
		fileOffset += dwWriteSize;
	}


	CloseHandle(hFile);

	return true;
}

bool ImportRebuild::writeZeroMemoryToFile(HANDLE hFile, DWORD fileOffset, DWORD size)
{
	bool retValue = false;
	PVOID zeromemory = calloc(size, 1);

	if (zeromemory)
	{
		retValue = ProcessAccessHelp::writeMemoryToFile(hFile, fileOffset, size, zeromemory);
		free(zeromemory);
	}
	else
	{
		retValue = false;
	}

	return retValue;
}

void ImportRebuild::clearAllData()
{
	if (pDosStub)
	{
		delete [] pDosStub;
		pDosStub = 0;
	}

	if (imageData)
	{
		delete [] imageData;
		imageData = 0;
	}

	delete pDosHeader;
	pDosHeader = 0;

	delete pNTHeader;
	pNTHeader = 0;

	vecSectionHeaders.clear();

	for (size_t i = 0; i < vecSectionData.size(); i++)
	{
		delete [] vecSectionData[i];
	}

	vecSectionData.clear();
}

bool ImportRebuild::addNewSection(char * sectionName, DWORD sectionSize, BYTE * sectionData)
{
	BYTE * newBuffer = 0;
	IMAGE_SECTION_HEADER pNewSection = {0};
	size_t lastSectionIndex = vecSectionHeaders.size() - 1;
	size_t nameLength = strlen(sectionName);

	if (nameLength > IMAGE_SIZEOF_SHORT_NAME)
	{
#ifdef DEBUG_COMMENTS
		Logger::debugLog(TEXT("addNewSection :: sectionname is too long %d\r\n"),nameLength);
#endif
		return false;
	}

	memcpy_s(pNewSection.Name, IMAGE_SIZEOF_SHORT_NAME, sectionName, nameLength);

	pNewSection.SizeOfRawData = alignValue(sectionSize, pNTHeader->OptionalHeader.FileAlignment);
	pNewSection.Misc.VirtualSize = alignValue(sectionSize, pNTHeader->OptionalHeader.SectionAlignment);

	pNewSection.PointerToRawData = alignValue(vecSectionHeaders[lastSectionIndex].PointerToRawData + vecSectionHeaders[lastSectionIndex].SizeOfRawData, pNTHeader->OptionalHeader.FileAlignment);
	pNewSection.VirtualAddress = alignValue(vecSectionHeaders[lastSectionIndex].VirtualAddress + vecSectionHeaders[lastSectionIndex].Misc.VirtualSize, pNTHeader->OptionalHeader.SectionAlignment);

	pNewSection.Characteristics = IMAGE_SCN_MEM_EXECUTE|IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_WRITE|IMAGE_SCN_CNT_CODE|IMAGE_SCN_CNT_INITIALIZED_DATA;

	vecSectionHeaders.push_back(pNewSection);

	if ( (sectionSize != pNewSection.SizeOfRawData) || (sectionData == 0) )
	{
		newBuffer = new BYTE[pNewSection.SizeOfRawData];
		ZeroMemory(newBuffer, pNewSection.SizeOfRawData);

		if (sectionData)
		{
			CopyMemory(newBuffer, sectionData, sectionSize);
		}
		
	}
	else
	{
		newBuffer = sectionData;
	}

	vecSectionData.push_back(newBuffer);

	return true;
}

bool ImportRebuild::loadTargetFile(const WCHAR * filepath)
{
	HANDLE hTargetFile = INVALID_HANDLE_VALUE;
	DWORD fileSize = 0;
	bool retValue = false;

	hTargetFile = CreateFile(filepath, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if(hTargetFile == INVALID_HANDLE_VALUE)
	{
#ifdef DEBUG_COMMENTS
		Logger::debugLog("loadTargetFile :: INVALID_HANDLE_VALUE %u\r\n",GetLastError());
#endif

		return false;
	}

	fileSize = (DWORD)ProcessAccessHelp::getFileSize(hTargetFile);

	if (!fileSize)
	{
		CloseHandle(hTargetFile);
		hTargetFile = 0;
		return false;
	}

	imageData = new BYTE[fileSize];

	if (!imageData)
	{
		retValue = false;
	}
	else
	{
		retValue = ProcessAccessHelp::readMemoryFromFile(hTargetFile, 0, fileSize, imageData);
	}

	CloseHandle(hTargetFile);
	hTargetFile = 0;

	return retValue;
}

DWORD ImportRebuild::alignValue(DWORD badValue, DWORD alignTo)
{
	return (((badValue + alignTo - 1) / alignTo) * alignTo);
}

DWORD ImportRebuild::convertRVAToOffsetVector(DWORD dwRVA)
{
	for (size_t i = 0; i < vecSectionHeaders.size(); i++)
	{
		if ((vecSectionHeaders[i].VirtualAddress <= dwRVA) && ((vecSectionHeaders[i].VirtualAddress + vecSectionHeaders[i].Misc.VirtualSize) > dwRVA))
		{
			return ((dwRVA - vecSectionHeaders[i].VirtualAddress) + vecSectionHeaders[i].PointerToRawData);
		}
	}

	return 0;
}

DWORD ImportRebuild::convertRVAToOffset(DWORD dwRVA)
{
	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNTHeader);

	for (WORD i = 0; i < pNTHeader->FileHeader.NumberOfSections; i++)
	{
		if ((pSectionHeader->VirtualAddress <= dwRVA) && ((pSectionHeader->VirtualAddress + pSectionHeader->Misc.VirtualSize) > dwRVA))
		{
			return ((dwRVA - pSectionHeader->VirtualAddress) + pSectionHeader->PointerToRawData);
		}
		pSectionHeader++;
	}

	return 0;
}

DWORD_PTR ImportRebuild::convertOffsetToRVAVector(DWORD dwOffset)
{
	for (size_t i = 0; i < vecSectionHeaders.size(); i++)
	{
		if ((vecSectionHeaders[i].PointerToRawData <= dwOffset) && ((vecSectionHeaders[i].PointerToRawData + vecSectionHeaders[i].SizeOfRawData) > dwOffset))
		{
			return ((dwOffset - vecSectionHeaders[i].PointerToRawData) + vecSectionHeaders[i].VirtualAddress);
		}
	}

	return 0;
}

DWORD ImportRebuild::convertOffsetToRVA(DWORD dwOffset)
{
	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNTHeader);

	for (WORD i = 0; i < pNTHeader->FileHeader.NumberOfSections; i++)
	{
		if ((pSectionHeader->PointerToRawData <= dwOffset) && ((pSectionHeader->PointerToRawData + pSectionHeader->SizeOfRawData) > dwOffset))
		{
			return ((dwOffset - pSectionHeader->PointerToRawData) + pSectionHeader->VirtualAddress);
		}
		pSectionHeader++;
	}

	return 0;
}

void ImportRebuild::updatePeHeader()
{
	size_t lastSectionIndex = vecSectionHeaders.size() - 1;

	pNTHeader->FileHeader.NumberOfSections = (WORD)(lastSectionIndex + 1);
	pNTHeader->OptionalHeader.SizeOfImage = vecSectionHeaders[lastSectionIndex].VirtualAddress + vecSectionHeaders[lastSectionIndex].Misc.VirtualSize;

	pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].VirtualAddress = 0;
	pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].Size = 0;

	if (pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress)
	{
		for (size_t i = 0; i < vecSectionHeaders.size(); i++)
		{
			if ((vecSectionHeaders[i].VirtualAddress <= pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress) && ((vecSectionHeaders[i].VirtualAddress + vecSectionHeaders[i].Misc.VirtualSize) > pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress))
			{
				//section must be read and writeable
				vecSectionHeaders[i].Characteristics |= IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_WRITE;
			}
		}

		pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress = 0;
		pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size = 0;
	}


	pNTHeader->OptionalHeader.NumberOfRvaAndSizes = 0x10;

	pNTHeader->OptionalHeader.SizeOfHeaders = alignValue(pDosHeader->e_lfanew + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + pNTHeader->FileHeader.SizeOfOptionalHeader + (pNTHeader->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER)), pNTHeader->OptionalHeader.FileAlignment);

}



bool ImportRebuild::buildNewImportTable(std::map<DWORD_PTR, ImportModuleThunk> & moduleList)
{
	createNewImportSection(moduleList);

	importSectionIndex = vecSectionHeaders.size() - 1;

	DWORD dwSize = fillImportSection(moduleList);

	if (!dwSize)
	{
		return false;
	}

	setFlagToIATSection((*moduleList.begin()).second.firstThunk);

	pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = vecSectionHeaders[importSectionIndex].VirtualAddress;
	pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = (DWORD)(numberOfImportDescriptors * sizeof(IMAGE_IMPORT_DESCRIPTOR));
	return true;
}

bool ImportRebuild::createNewImportSection(std::map<DWORD_PTR, ImportModuleThunk> & moduleList)
{
	char sectionName[9] = {0};
	size_t i = 0;

	//DWORD sectionSize = calculateMinSize(moduleList);
	calculateImportSizes(moduleList);

	if (wcslen(ConfigurationHolder::getConfigObject(IAT_SECTION_NAME)->valueString) > IMAGE_SIZEOF_SHORT_NAME)
	{
		strcpy_s(sectionName, sizeof(sectionName), ".SCY");
	}
	else
	{
		wcstombs_s(&i, sectionName, sizeof(sectionName), ConfigurationHolder::getConfigObject(IAT_SECTION_NAME)->valueString, _TRUNCATE);
	}


	return addNewSection(sectionName, (DWORD)sizeOfImportSection, 0);
}

/*DWORD ImportRebuild::calculateMinSize(std::map<DWORD_PTR, ImportModuleThunk> & moduleList)
{
	DWORD dwSize = 0;
	std::map<DWORD_PTR, ImportModuleThunk>::iterator mapIt;
	std::map<DWORD_PTR, ImportThunk>::iterator mapIt2;

	dwSize = (DWORD)((moduleList.size() + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR)); //last is zero'ed

	for ( mapIt = moduleList.begin() ; mapIt != moduleList.end(); mapIt++ )
	{

		//dwSize += (DWORD)((*mapIt).second.thunkList.size() + sizeof(IMAGE_IMPORT_BY_NAME));
		dwSize += (DWORD)(wcslen((*mapIt).second.moduleName) + 1);

		for ( mapIt2 = (*mapIt).second.thunkList.begin() ; mapIt2 != (*mapIt).second.thunkList.end(); mapIt2++ )
		{
			if((*mapIt2).second.name[0] != '\0')
			{
				dwSize += sizeof(IMAGE_IMPORT_BY_NAME);
				dwSize += (DWORD)strlen((*mapIt2).second.name);
			}
		}
	}

	return dwSize;
}*/

BYTE * ImportRebuild::getMemoryPointerFromRVA(DWORD_PTR dwRVA)
{
	DWORD_PTR offset = convertRVAToOffsetVector((DWORD)dwRVA);

	for (size_t i = 0; i < vecSectionHeaders.size(); i++)
	{
		if ((vecSectionHeaders[i].PointerToRawData <= offset) && ((vecSectionHeaders[i].PointerToRawData + vecSectionHeaders[i].SizeOfRawData) > offset))
		{
			return (BYTE *)((DWORD_PTR)vecSectionData[i] + (offset - vecSectionHeaders[i].PointerToRawData));
		}
	}

	return 0;
}

DWORD ImportRebuild::fillImportSection( std::map<DWORD_PTR, ImportModuleThunk> & moduleList )
{
	std::map<DWORD_PTR, ImportModuleThunk>::iterator mapIt;
	std::map<DWORD_PTR, ImportThunk>::iterator mapIt2;
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = 0;
	PIMAGE_IMPORT_BY_NAME pImportByName = 0;
	PIMAGE_THUNK_DATA pThunk = 0;
	ImportModuleThunk * importModuleThunk = 0;
	ImportThunk * importThunk = 0;

	size_t stringLength = 0;
	DWORD_PTR lastRVA = 0;

	BYTE * sectionData = vecSectionData[importSectionIndex];
	DWORD offset = 0;

	pImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)(sectionData);

	//skip the IMAGE_IMPORT_DESCRIPTOR
	offset += (DWORD)(numberOfImportDescriptors * sizeof(IMAGE_IMPORT_DESCRIPTOR));

	for ( mapIt = moduleList.begin() ; mapIt != moduleList.end(); mapIt++ )
	{
		importModuleThunk = &((*mapIt).second);

		stringLength = addImportDescriptor(importModuleThunk, offset);

#ifdef DEBUG_COMMENTS
		Logger::debugLog("fillImportSection :: importDesc.Name %X\r\n", pImportDescriptor->Name);
#endif

		offset += (DWORD)stringLength; //stringLength has null termination char

		pImportByName = (PIMAGE_IMPORT_BY_NAME)((DWORD_PTR)sectionData + offset);

		//pThunk = (PIMAGE_THUNK_DATA)(getMemoryPointerFromRVA(importModuleThunk->firstThunk));

		lastRVA = importModuleThunk->firstThunk - sizeof(DWORD_PTR);

		for ( mapIt2 = (*mapIt).second.thunkList.begin() ; mapIt2 != (*mapIt).second.thunkList.end(); mapIt2++ )
		{
			importThunk = &((*mapIt2).second);

			pThunk = (PIMAGE_THUNK_DATA)(getMemoryPointerFromRVA(importThunk->rva));

			//check wrong iat pointer
			if (!pThunk)
			{
#ifdef DEBUG_COMMENTS
				Logger::debugLog(TEXT("fillImportSection :: Failed to get pThunk RVA: %X\n"), importThunk->rva);
#endif
				return 0;
			}

			if ((lastRVA + sizeof(DWORD_PTR)) != importThunk->rva)
			{
				//add additional import desc
				addSpecialImportDescriptor(importThunk->rva);
			}
			lastRVA = importThunk->rva;

#ifdef DEBUG_COMMENTS
			Logger::debugLog(TEXT("fillImportSection :: importThunk %X pThunk %X pImportByName %X offset %X\n"), importThunk,pThunk,pImportByName,offset);
#endif
			stringLength = addImportToImportTable(importThunk, pThunk, pImportByName, offset);

			offset += (DWORD)stringLength; //is 0 bei import by ordinal
			pImportByName = (PIMAGE_IMPORT_BY_NAME)((DWORD_PTR)pImportByName + stringLength);
		}

		pImportDescriptor++;
	}

	return offset;
}

bool ImportRebuild::rebuildImportTable(const WCHAR * targetFilePath, const WCHAR * newFilePath, std::map<DWORD_PTR, ImportModuleThunk> & moduleList)
{
	bool retValue = false;

	if (loadTargetFile(targetFilePath))
	{
		splitTargetFile();

		retValue = buildNewImportTable(moduleList);

		if (retValue)
		{
			retValue = saveNewFile(newFilePath);
		}

		clearAllData();

		return retValue;
	}
	else
	{
#ifdef DEBUG_COMMENTS
		Logger::debugLog(TEXT("rebuildImportTable ::Failed to load target %s\n"), targetFilePath);
#endif
		return false;
	}
}

void ImportRebuild::setFlagToIATSection(DWORD_PTR iatAddress)
{
	for (size_t i = 0; i < vecSectionHeaders.size(); i++)
	{
		if ((vecSectionHeaders[i].VirtualAddress <= iatAddress) && ((vecSectionHeaders[i].VirtualAddress + vecSectionHeaders[i].Misc.VirtualSize) > iatAddress))
		{
			//section must be read and writeable
			vecSectionHeaders[i].Characteristics |= IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_WRITE;
		}
	}
}

size_t ImportRebuild::addImportToImportTable( ImportThunk * pImport, PIMAGE_THUNK_DATA pThunk, PIMAGE_IMPORT_BY_NAME pImportByName, DWORD sectionOffset)
{
	size_t stringLength = 0;

	if(pImport->name[0] == '\0')
	{
		pThunk->u1.AddressOfData = (IMAGE_ORDINAL(pImport->ordinal) | IMAGE_ORDINAL_FLAG);
	}
	else
	{
		pImportByName->Hint = pImport->hint;

		stringLength = strlen(pImport->name) + 1;
		memcpy(pImportByName->Name, pImport->name, stringLength);

		pThunk->u1.AddressOfData = convertOffsetToRVAVector(vecSectionHeaders[importSectionIndex].PointerToRawData + sectionOffset);

		if (!pThunk->u1.AddressOfData)
		{
#ifdef DEBUG_COMMENTS
			Logger::debugLog("addImportToImportTable :: failed to get AddressOfData %X %X\n",vecSectionHeaders[importSectionIndex].PointerToRawData, sectionOffset);
#endif
		}

#ifdef DEBUG_COMMENTS
		Logger::debugLog("addImportToImportTable :: pThunk->u1.AddressOfData %X %X %X\n",pThunk->u1.AddressOfData, pThunk,  vecSectionHeaders[importSectionIndex].PointerToRawData + sectionOffset);
#endif
		stringLength += sizeof(WORD);
	}

	return stringLength;
}

size_t ImportRebuild::addImportDescriptor(ImportModuleThunk * pImportModule, DWORD sectionOffset)
{
	char dllName[MAX_PATH];
	size_t stringLength = 0;

	wcstombs_s(&stringLength, dllName, (size_t)MAX_PATH, pImportModule->moduleName, (size_t)MAX_PATH);

	memcpy((vecSectionData[importSectionIndex] + sectionOffset), dllName, stringLength); //copy module name to section

	pImportDescriptor->FirstThunk = (DWORD)pImportModule->firstThunk;
	pImportDescriptor->Name = (DWORD)convertOffsetToRVAVector(vecSectionHeaders[importSectionIndex].PointerToRawData + sectionOffset);

	return stringLength;
}

void ImportRebuild::addSpecialImportDescriptor(DWORD_PTR rvaFirstThunk)
{
	PIMAGE_IMPORT_DESCRIPTOR oldID = pImportDescriptor;
	pImportDescriptor++;

	pImportDescriptor->FirstThunk = (DWORD)rvaFirstThunk;
	pImportDescriptor->Name = oldID->Name;
}

void ImportRebuild::calculateImportSizes(std::map<DWORD_PTR, ImportModuleThunk> & moduleList)
{
	std::map<DWORD_PTR, ImportModuleThunk>::iterator mapIt;
	std::map<DWORD_PTR, ImportThunk>::iterator mapIt2;
	DWORD_PTR lastRVA = 0;

	numberOfImportDescriptors = 0;
	sizeOfImportSection = 0;
	sizeOfApiAndModuleNames = 0;

	numberOfImportDescriptors = moduleList.size() + 1; //last is zero'd

	for ( mapIt = moduleList.begin() ; mapIt != moduleList.end(); mapIt++ )
	{
		lastRVA = (*mapIt).second.firstThunk - sizeof(DWORD_PTR);

		sizeOfApiAndModuleNames += (DWORD)(wcslen((*mapIt).second.moduleName) + 1);

		for ( mapIt2 = (*mapIt).second.thunkList.begin() ; mapIt2 != (*mapIt).second.thunkList.end(); mapIt2++ )
		{
			if ((lastRVA + sizeof(DWORD_PTR)) != (*mapIt2).second.rva)
			{
				numberOfImportDescriptors++; //add additional import desc
			}

			if((*mapIt2).second.name[0] != '\0')
			{
				sizeOfApiAndModuleNames += sizeof(WORD); //Hint from IMAGE_IMPORT_BY_NAME
				sizeOfApiAndModuleNames += (DWORD)(strlen((*mapIt2).second.name) + 1);
			}

			lastRVA = (*mapIt2).second.rva;
		}
	}

	sizeOfImportSection = sizeOfApiAndModuleNames + (numberOfImportDescriptors * sizeof(IMAGE_IMPORT_DESCRIPTOR));
}