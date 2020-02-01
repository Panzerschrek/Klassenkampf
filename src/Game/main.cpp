#include "Log.hpp"
#include "Host.hpp"


int main()
{
	using namespace KK;

	try
	{
		Host host;
		while(!host.Loop()){}
	}
	catch(const std::exception& ex)
	{
		Log::FatalError("Exception throwed: ", ex.what());
	}
}
