#pragma once
#include <cstddef>
#include <memory>


namespace KK
{

class MemoryMappedFile;
using MemoryMappedFilePtr= std::unique_ptr<const MemoryMappedFile>;

class MemoryMappedFile
{
public:
	static MemoryMappedFilePtr Create(const char* const file_name);

public:
	~MemoryMappedFile();

	const void* Data() const { return data_; }
	size_t Size() const { return size_; }

public:
	using FileDescriptor= int;

private:
	MemoryMappedFile(const void* data, size_t size, FileDescriptor file_descriptor);

private:
	const void* const data_;
	const size_t size_;
	FileDescriptor file_descriptor_;
};

} // namespace KK
