#pragma once

#include <dlfcn.h>

namespace x3me
{
namespace os_utils
{
// Dynamic Library Loader
class dl_loader
{
	void* lib_handle_;

public:
	explicit dl_loader(const char* dl_file_path)
	: lib_handle_(dlopen(dl_file_path, RTLD_LAZY))
	{}

	dl_loader(const dl_loader&) = delete;
	dl_loader& operator =(const dl_loader&) = delete;

	dl_loader(dl_loader&& rhs) : lib_handle_(rhs.lib_handle_)
	{
		rhs.lib_handle_ = nullptr;
	}

	dl_loader& operator =(dl_loader&& rhs)
	{
		if (this != &rhs)
		{
			close();
			lib_handle_		= rhs.lib_handle_;
			rhs.lib_handle_	= nullptr;
		}
		return *this;
	}

	~dl_loader()
	{
		close();
	}

	void close()
	{
		if (lib_handle_)
		{
			dlclose(lib_handle_);
			lib_handle_ = nullptr;
		}
	}

	bool lib_loaded() const
	{
		return !!lib_handle_;
	}

	template <typename FuncType>
	FuncType* load_func(const char* func_name)
	{
		FuncType* f = nullptr;
		if (lib_handle_)
		{			
			*reinterpret_cast<void**>(&f) = dlsym(lib_handle_, func_name);
		}
		return f;
	}
};

} // namespace os_utils
} // namespace x3me
