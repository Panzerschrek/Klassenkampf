#include "Host.hpp"


int main()
{
	using namespace KK;

	Host host;
	while(!host.Loop()){}
}
