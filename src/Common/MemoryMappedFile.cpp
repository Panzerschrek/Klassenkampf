#include "MemoryMappedFile.hpp"
#include <fcntl.h>

#ifdef WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include <iostream>


namespace KK
{

MemoryMappedFilePtr MemoryMappedFile::Create(const char* const file_name)
{
#ifdef WIN32
	// TODO - maybe use "CreateFileW"?
	const HANDLE file_descriptor=
		::CreateFileA(
			file_name,
			GENERIC_READ,
			0,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr);
	if(file_descriptor == INVALID_HANDLE_VALUE)
	{
		std::cout << "Error, opening file \"" << file_name << "\". Error code: " << GetLastError() << std::endl;
		return nullptr;
	}

	LARGE_INTEGER file_size;
	if(GetFileSizeEx(file_descriptor, &file_size) == 0)
	{
		std::cout << "Error, getting file size. Error code: " << GetLastError() << std::endl;
		::CloseHandle(file_descriptor);
		return nullptr;
	}

	const HANDLE file_mapping_handle=
		CreateFileMappingA(
			file_descriptor,
			nullptr,
			PAGE_READONLY,
			file_size.HighPart,
			file_size.LowPart,
			nullptr);

	const void* const data= ::MapViewOfFile(file_mapping_handle, FILE_MAP_READ, 0u, 0, file_size.QuadPart);
	if(data == nullptr)
	{
		std::cout << "Error, mapping file \"" << file_name << "\". Error code: " << GetLastError() << std::endl;
		::CloseHandle(file_mapping_handle);
		::CloseHandle(file_descriptor);
		return nullptr;
	}

	return MemoryMappedFilePtr(new MemoryMappedFile(data, file_size.QuadPart, file_descriptor, file_mapping_handle));
#else
	const FileDescriptor file_descriptor= ::open(file_name, O_LARGEFILE);
	if( file_descriptor == -1 )
	{
		std::cout << "Error, opening file \"" << file_name << "\". Error code: " << errno << std::endl;
		return nullptr;
	}

	struct stat file_statistics;
	const int stat_result= ::fstat(file_descriptor, &file_statistics);
	if(stat_result != 0)
	{
		std::cout << "Error, getting file size. Error code: " << errno << std::endl;
		::close(file_descriptor);
		return nullptr;
	}

	const size_t file_size= file_statistics.st_size;

	const void* const data= ::mmap(nullptr, file_size, PROT_READ, MAP_SHARED, file_descriptor, 0u);
	if(data == nullptr)
	{
		std::cout << "Error, mapping file \"" << file_name << "\". Error code: " << errno << std::endl;
		::close(file_descriptor);
		return nullptr;
	}

	return MemoryMappedFilePtr(new MemoryMappedFile(data, file_size, file_descriptor, 0));
#endif
}

MemoryMappedFile::MemoryMappedFile(
	const void* const data,
	const size_t size,
	const FileDescriptor file_descriptor0,
	const FileDescriptor file_descriptor1)
	: data_(data)
	, size_(size)
	, file_descriptor0_(file_descriptor0)
	, file_descriptor1_(file_descriptor1)
{}

MemoryMappedFile::~MemoryMappedFile()
{
#ifdef WIN32
	::UnmapViewOfFile(data_);
	::CloseHandle(file_descriptor1_);
	::CloseHandle(file_descriptor0_);
#else
	::munmap(const_cast<void*>(data_), size_);
	::close(file_descriptor0_);
#endif
}

} // namespace KK
