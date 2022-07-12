#include <iostream>
#include <cctype>

#include "Common.hpp"


int main()
{
	App::Client c;
	c.Connect( "127.0.0.1" , 60000 );
	//c.Connect( "localhost" , 60000 );

	//char ch {};

	int cnt = 0;

	bool bQuit = false;
	//while ( !bQuit )
	//{
		/*if ( isdigit( std::cin.peek() ) )
		{
			std::cin >> std::ws >> ch;
			std::cin.ignore();

			if ( ch == '1' )
				c.PingServer();
			if ( ch == '2' )
				c.MessageAll();
			if ( ch == '3' )
				c.Vec_Msg();
			if ( ch == '4' )
				bQuit = true;

		}
		else
		{
			std::cin.ignore();
		}*/
	while ( !bQuit )
	{
		if ( c.IsConnected() )
		{
			if ( !c.Incoming().empty() )
			{
				auto && msg = c.Incoming().pop_front().msg;

				switch ( msg.header.id )
				{
					case App::AppMsg::Vec_Msg:
					{
						std::array<char[ 256 ] , 4> vec {};
						msg >> vec;

						std::cout << "Server Sent Vec: ";
						for ( auto && var : vec )
						{
							std::cout << var << ' ';
						}
						std::cout.put( '\n' );
						c.Vec_Msg();
					}
					break;

					case App::AppMsg::ServerAccept:
					{
						// Server has responded to a ping request
						std::cout << "Server Accepted Connection\n";
						c.Vec_Msg();
					}
					break;

					case App::AppMsg::ServerPing:
					{
						// Server has responded to a ping request
						std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();
						std::chrono::system_clock::time_point timeThen;
						msg >> timeThen;
						std::cout << "Ping: " << std::chrono::duration<double>( timeNow - timeThen ).count() << "\n";
						c.Vec_Msg();
					}
					break;

					case App::AppMsg::ServerMessage:
					{
						// Server has responded to a ping request
						uint32_t clientID {};
						msg >> clientID;
						std::cout << "Hello from [" << clientID << "]\n";
					}
					break;
				}
			}
		}
		else
		{
			std::cout << "Server Down\n";
			bQuit = true;
		}
	}
}


