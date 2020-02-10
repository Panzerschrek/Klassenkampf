#pragma once
#include <cstddef>
#include <memory>
#include <string_view>


namespace KK
{

class MemoryMappedFile;
using MemoryMappedFilePtr= std::unique_ptr<const MemoryMappedFile>;

class MemoryMappedFile
{
public:
	static MemoryMappedFilePtr Create(std::string_view file_name);

public:
	~MemoryMappedFile();

	const void* Data() const { return data_; }
	size_t Size() const { return size_; }

public:
#ifdef WIN32
	using FileDescriptor= void*;
#else
	using FileDescriptor= int;
#endif

private:
	MemoryMappedFile(const void* data, size_t size, FileDescriptor file_descriptor0, FileDescriptor file_descriptor1);

private:
	const void* const data_;
	const size_t size_;
	const FileDescriptor file_descriptor0_;
	const FileDescriptor file_descriptor1_;
};

} // namespace KK
