#include "Common.hpp"

int main()
{
	App::Server server( "AppDB.db" , 60000 );
	server.Start();

	while ( true )
	{
		server.Update( -1 , true );
	}
}