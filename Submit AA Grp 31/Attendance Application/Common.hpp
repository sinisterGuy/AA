#pragma once

#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <random>

#include "olcPGEX_Network.h"
#include "Bcrypt/include/bcrypt.h"
#include "sqlite3.h"

namespace App
{
	enum struct Entity
	{
		None = -1 ,
		Student ,
		Professor ,
		Admin ,
	};

	enum struct AppMsg : uint32_t
	{
		// Errors
		Result_Ok ,
		Result_Err ,

		// Common
		Login ,
		Course ,
		Change_Pass ,

		// For Students
		Give_Att ,
		Stud_Stats ,

		// For Teachers
		Take_Att ,
		Stop_Att ,
		Prof_Stats ,
		Proxy ,

		// For Admin
		Add_Course ,
		View_Course ,
		Edit_Course ,
		Del_Course ,

		ServerAccept ,
		ServerDeny ,
		ServerMessage ,
		Server_Down ,
	};

	constexpr size_t SIZE { 256 };
	typedef char String[ SIZE ];

	template <size_t N = 1>
	using Table = std::vector<std::array<String , N>>;

	using Msg = olc::net::message<AppMsg>;

	template <size_t N = 1>
	int callback( void * arr , int nCols , char * colVal[] , char * colName[] )
	{
		Table<N> & vec = *reinterpret_cast< Table<N> * >( arr );
		vec.emplace_back();

		for ( int i {}; i < nCols; ++i )
		{
			strcpy_s( vec.back()[ i ] , colVal[ i ] ? colVal[ i ] : "" );
		}

		return 0;
	}

	// Server Class
	class Server : public olc::net::server_interface<AppMsg>
	{
	private:
		sqlite3 * m_DB { nullptr };

		std::mt19937_64 gen { std::random_device {}( ) };
		std::uniform_int_distribution<int32_t> dis { 100'000 , 999'999 };


		template <size_t N = 1>
		Table<N> exec( const std::string & cmd )
		{
			Table<N> vec {};
			char * messageError;

			try
			{
				int rc = sqlite3_exec( m_DB , cmd.c_str() , callback<N> , &vec , &messageError );
				if ( rc != SQLITE_OK )
				{
					std::cerr << "Error : " << messageError << std::endl;
					sqlite3_free( messageError );
				}
				else
					std::cout << "Successfull" << std::endl;
			}
			catch ( const std::exception & e )
			{
				std::cerr << e.what();
			}

			return vec;
		}

	public:
		Server( const char * path , uint16_t nPort ) : olc::net::server_interface<AppMsg>( nPort )
		{
			int rc = sqlite3_open( path , &m_DB );

			if ( rc )
			{
				std::cerr << "[SERVER ERROR!] Can't open database: " << sqlite3_errmsg( m_DB ) << '\n';
				exit( 0 );
			}

			sqlite3_exec( m_DB , "PRAGMA foreign_keys = on;" , nullptr , nullptr , nullptr);
		}

		~Server()
		{
			sqlite3_close( m_DB );
		}

	private:
		bool OnClientConnect( std::shared_ptr<olc::net::connection<AppMsg>> client ) override
		{
			// For now we will allow all
			return true;
		}

		virtual void OnClientValidated( std::shared_ptr<olc::net::connection<AppMsg>> client )
		{
			olc::net::message<AppMsg> msg;
			msg.header.id = AppMsg::ServerAccept;
			client->Send( msg );
		}

		// Called when a client appears to have disconnected
		virtual void OnClientDisconnect( std::shared_ptr<olc::net::connection<AppMsg>> client )
		{
			std::cout << "Removing client [" << client->GetID() << "]\n";
		}

		// Called when a message arrives
		virtual void OnMessage( std::shared_ptr<olc::net::connection<AppMsg>> client , olc::net::message<AppMsg> & msg )
		{
			switch ( msg.header.id )
			{
				case AppMsg::Login:
				{
					std::cout << "[" << client->GetID() << "]: Login\n";

					String str {};
					std::string id {} , pass {};
					Entity entity {};

					msg >> str;
					pass = str;
					msg >> str;
					id = str;
					msg >> entity;

					client->Send( Login( entity , id , pass ) );
				}
				break;

				case AppMsg::Course:
				{
					std::cout << "[" << client->GetID() << "]: Course\n";

					String str {};
					std::string id {};
					Entity entity {};

					msg >> str;
					id = str;
					msg >> entity;

					client->Send( Course( entity , id ) );
				}
				break;

				case AppMsg::Change_Pass:
				{
					std::cout << "[" << client->GetID() << "]: Change_Pass\n";

					String str {};
					std::string id {} , old {} , pass {};
					Entity entity {};

					msg >> str;
					pass = str;
					msg >> str;
					old = str;
					msg >> str;
					id = str;
					msg >> entity;

					client->Send( Change_Pass( entity , id , old , pass ) );
				}
				break;

				case AppMsg::Give_Att:
				{
					std::cout << "[" << client->GetID() << "]: Give_Att\n";

					String str {};
					std::string id {} , cid {};
					int32_t code {};

					msg >> code;
					msg >> str;
					cid = str;
					msg >> str;
					id = str;

					client->Send( Give_Att( id , cid , client->GetIP() , code));
				}
				break;

				case AppMsg::Stud_Stats:
				{
					std::cout << "[" << client->GetID() << "]: Stud_Stats\n";

					String str {};
					std::string id {} , cid {};

					msg >> str;
					cid = str;
					msg >> str;
					id = str;

					client->Send( Stud_Stats( id , cid ) );
				}
				break;

				case AppMsg::Take_Att:
				{
					std::cout << "[" << client->GetID() << "]: Take_Att\n";

					String str {};
					std::string cid {};

					msg >> str;
					cid = str;

					client->Send( Take_Att( cid , dis( gen ) ) );
				}
				break;

				case AppMsg::Stop_Att:
				{
					std::cout << "[" << client->GetID() << "]: Stop_Att\n";

					String str {};
					std::string cid {};

					msg >> str;
					cid = str;

					client->Send( Stop_Att( cid ) );
				}
				break;

				case AppMsg::Prof_Stats:
				{
					std::cout << "[" << client->GetID() << "]: Prof_Stats\n";

					String str {};
					std::string cid {};

					msg >> str;
					cid = str;

					client->Send( Prof_Stats( cid ) );
				}
				break;

				case AppMsg::Proxy:
				{
					std::cout << "[" << client->GetID() << "]: Proxy\n";

					String str {};
					std::string cid {};

					msg >> str;
					cid = str;

					client->Send( Proxy( cid ) );
				}
				break;

				case AppMsg::Add_Course:
				{
					std::cout << "[" << client->GetID() << "]: Add_Course\n";

					String str {};
					std::vector<std::string> details { 6 };

					for ( int32_t i = ( int32_t ) details.size() - 1; i >= 0; --i )
					{
						msg >> str;
						details[ i ] = str;
					}

					client->Send( Add_Course( details ) );
				}
				break;

				case AppMsg::View_Course:
				{
					std::cout << "[" << client->GetID() << "]: View_Course\n";

					String str {};
					std::string cid {};

					msg >> str;
					cid = str;

					client->Send( View_Course( cid ) );
				}
				break;

				case AppMsg::Edit_Course:
				{
					std::cout << "[" << client->GetID() << "]: Edit_Course\n";

					String str {};
					std::vector<std::string> details { 6 };
					std::string cid {};

					for ( int32_t i = ( int32_t ) details.size() - 1; i >= 0; --i )
					{
						msg >> str;
						details[ i ] = str;
					}

					msg >> str;
					cid = str;

					client->Send( Edit_Course( cid , details ) );
				}
				break;

				case AppMsg::Del_Course:
				{
					std::cout << "[" << client->GetID() << "]: Del_Course\n";

					String str {};
					std::string cid {};

					msg >> str;
					cid = str;

					client->Send( Del_Course( cid ) );
				}
				break;
			}
		}

		Msg Login(
			Entity entity ,
			const std::string id ,
			const std::string pass )
		{
			Msg msg {};

			std::string db {} , pk {};

			switch ( entity )
			{
				case App::Entity::Student:
					db = "STUDENT ";
					pk = "S_ID";
					break;

				case App::Entity::Professor:
					db = "TEACHER ";
					pk = "T_ID";
					break;

				case App::Entity::Admin:
					db = "ADMIN ";
					pk = "A_ID";
					break;

				default:
					msg.header.id = AppMsg::Result_Err;
					return msg;
					break;
			}

			std::string sql =
				"SELECT EXISTS( "
				"SELECT 1 "
				"FROM " + db + " "
				"WHERE " + pk + " = '" + id + "'); ";

			auto vec = exec<1>( sql );

			if ( vec[ 0 ][ 0 ][ 0 ] == '0' )
			{
				std::cout << "Wrong UserID\n" << std::endl;
				msg.header.id = AppMsg::Result_Err;
				return msg;
			}

			sql =
				"SELECT Password "
				"FROM " + db + " "
				"WHERE " + pk + " = '" + id + "';";

			vec = exec<1>( sql );

			if ( bcrypt::validatePassword( pass , vec[ 0 ][ 0 ] ) )
			{
				std::cout << "Login successfull" << std::endl;
				msg.header.id = AppMsg::Result_Ok;
			}
			else
			{ // No record selected
				std::cout << "Unable to login" << std::endl;
				msg.header.id = AppMsg::Result_Err;
			}

			return msg;
		}

		Msg Course(
			Entity entity ,
			const std::string & id )
		{
			Msg msg {};
			std::string sql {};

			switch ( entity )
			{
				case App::Entity::Student:
					sql =
						"SELECT C.C_ID "
						"FROM COURSE C "
						"WHERE (C.D_ID, C.Semester) = ( "
						"SELECT S.D_ID, S.Semester "
						"FROM STUDENT S "
						"WHERE S.S_ID = '" + id + "');";
					break;

				case App::Entity::Professor:
					sql =
						"SELECT C.C_ID "
						"FROM COURSE C "
						"WHERE C.T_ID1 = '" + id + "' "
						"OR    C.T_ID2 = '" + id + "';";
					break;

				default:
					msg.header.id = AppMsg::Result_Err;
					return msg;
					break;
			}

			auto vec = exec<1>( sql );

			msg.header.id = AppMsg::Result_Ok;

			for ( int32_t i = ( int32_t ) vec.size() - 1; i >= 0; --i )
			{
				msg << vec[ i ][ 0 ];
			}

			msg << vec.size();

			return msg;
		}

		Msg Change_Pass(
			Entity entity ,
			const std::string & id ,
			const std::string & old ,
			const std::string & pass )
		{
			Msg msg {};
			std::string db {} , pk {};

			switch ( entity )
			{
				case App::Entity::Student:
					db = "STUDENT ";
					pk = "S_ID";
					break;

				case App::Entity::Professor:
					db = "TEACHER ";
					pk = "T_ID";
					break;

				case App::Entity::Admin:
					db = "ADMIN ";
					pk = "A_ID";
					break;

				default:
					msg.header.id = AppMsg::Result_Err;
					return msg;
					break;
			}

			std::string sql =
				"SELECT Password "
				"FROM " + db + " "
				"WHERE " + pk + " = '" + id + "';";

			auto vec = exec<1>( sql );

			// TODO : validate

			if ( !bcrypt::validatePassword( old , vec[ 0 ][ 0 ] ) )
			//if ( false )
			{
				msg.header.id = AppMsg::Result_Err;
				return msg;
			}

			sql =
				"UPDATE " + db + " "
				"SET Password = '" + bcrypt::generateHash( pass , 15 ) + "' "
				//"SET Password = '" + pass + "' "
				"WHERE " + pk + " = '" + id + "';";

			int rc = sqlite3_exec( m_DB , sql.c_str() , nullptr , nullptr , nullptr );

			if ( rc != SQLITE_OK )
			{
				std::cout << "Unable to update password" << std::endl;
				msg.header.id = AppMsg::Result_Err;
			}
			else
			{
				std::cout << "Password updatded" << std::endl;
				msg.header.id = AppMsg::Result_Ok;
			}

			return msg;
		}

		Msg Give_Att(
			const std::string & id ,
			const std::string & cid ,
			const std::string & ip ,
			int32_t code )
		{
			Msg msg {};

			std::string sql =
				"SELECT EXISTS ( "
				"SELECT 1 "
				"FROM ATTENDANCE_CODE "
				"WHERE C_ID = '" + cid + "' "
				"AND   CODE =  " + std::to_string( code ) + ");";

			auto vec = exec<1>( sql );

			if ( vec[ 0 ][ 0 ][ 0 ] == '0' )
			{
				msg.header.id = AppMsg::Result_Err;
				String str { "Code Doesn't Match!" };
				msg << str;
				return msg;
			}

			sql =
				"UPDATE ATTENDANCE "
				"SET P_A = 'P', IP_Address = '" + ip + "' "
				"WHERE S_ID = '" + id + "' "
				"AND   C_ID = '" + cid + "' "
				"AND Lesson = date();";

			char * errMsg {};
			int exit = sqlite3_exec( m_DB , sql.c_str() , nullptr , nullptr , &errMsg );

			if ( exit != SQLITE_OK )
			{
				std::cout << errMsg << '\n';
				sqlite3_free( errMsg );
				msg.header.id = AppMsg::Result_Err;
			}
			else
			{
				msg.header.id = AppMsg::Result_Ok;
			}

			return msg;
		}

		Msg Stud_Stats(
			const std::string & id ,
			const std::string & cid )
		{
			Msg msg {};

			std::string sql =
				"SELECT Lesson, P_A "
				"FROM ATTENDANCE "
				"WHERE S_ID = '" + id + "' "
				"AND   C_ID = '" + cid + "' "
				"ORDER BY Lesson DESC;";

			auto vec = exec<2>( sql );

			msg.header.id = AppMsg::Result_Ok;

			for ( int32_t i = ( int32_t ) vec.size() - 1; i >= 0; --i )
			{
				msg << vec[ i ];
			}

			msg << vec.size();

			return msg;
		}

		Msg Take_Att( const std::string & cid , int32_t code )
		{
			Fill_Att( cid );

			std::string sql =
				"INSERT INTO ATTENDANCE_CODE "
				"VALUES ('" + cid + "', " + std::to_string( code ) + ");";

			char * errMsg {};
			int exit = sqlite3_exec( m_DB , sql.c_str() , nullptr , nullptr , &errMsg );
			Msg msg {};

			if ( exit != SQLITE_OK )
			{
				std::cout << errMsg << std::endl;
				sqlite3_free( errMsg );
				msg.header.id = AppMsg::Result_Err;
			}
			else
			{
				std::cout << "Successfull!" << std::endl;
				msg << code;
				msg.header.id = AppMsg::Result_Ok;
			}

			return msg;
		}

		AppMsg Fill_Att( const std::string & cid )
		{
			std::string sql =
				"INSERT INTO ATTENDANCE (S_ID, C_ID, P_A, Lesson) "
				"SELECT S_ID, '" + cid + "', 'A', date() "
				"FROM STUDENT "
				"WHERE (Semester, D_ID) = ( "
				"SELECT Semester, D_ID "
				"FROM COURSE "
				"WHERE C_ID = '" + cid + "');";

			char * messageError {};
			int rc = sqlite3_exec( m_DB , sql.c_str() , nullptr , nullptr , &messageError );

			if ( rc != SQLITE_OK )
			{
				std::cout << messageError << std::endl;
				sqlite3_free( messageError );
				return AppMsg::Result_Err;
			}
			else
			{
				std::cout << "Successfull!" << std::endl;
				return AppMsg::Result_Ok;
			}
		}

		Msg Stop_Att( const std::string & cid )
		{
			std::string sql =
				"DELETE FROM ATTENDANCE_CODE "
				"WHERE C_ID = '" + cid + "';";

			char * errMsg {};
			int exit = sqlite3_exec( m_DB , sql.c_str() , nullptr , nullptr , &errMsg );

			Msg msg {};

			if ( exit != SQLITE_OK )
			{
				std::cout << errMsg << std::endl;
				sqlite3_free( errMsg );
				msg.header.id = AppMsg::Result_Err;
			}
			else
			{
				std::cout << "Successfull!" << std::endl;
				msg.header.id = AppMsg::Result_Ok;
			}

			return msg;
		}

		Msg Prof_Stats( const std::string & cid )
		{
			std::string sql =
				"SELECT Lesson, S_ID, P_A "
				"FROM ATTENDANCE "
				"WHERE C_ID = '" + cid + "' "
				"ORDER BY Lesson DESC;";

			auto vec = exec<3>( sql );

			Msg msg {};
			msg.header.id = AppMsg::Result_Ok;

			for ( int32_t i = ( int32_t ) vec.size() - 1; i >= 0; --i )
			{
				msg << vec[ i ];
			}

			msg << vec.size();

			return msg;
		}

		Msg Proxy( const std::string & cid )
		{
			std::string sql =
				"SELECT Lesson, S_ID, IP_Address "
				"FROM ATTENDANCE "
				"WHERE (Lesson, IP_Address) IN ( "
				"SELECT Lesson, IP_Address "
				"FROM ATTENDANCE "
				"WHERE P_A = 'P' "
				"GROUP BY Lesson, IP_Address "
				"HAVING COUNT(*) > 1) "
				"AND C_ID = '" + cid + "' "
				"ORDER BY Lesson DESC, S_ID ASC;";

			auto vec = exec<3>( sql );

			Msg msg {};
			msg.header.id = AppMsg::Result_Ok;

			for ( int32_t i = ( int32_t ) vec.size() - 1; i >= 0; --i )
			{
				msg << vec[ i ];
			}

			msg << vec.size();

			return msg;
		}

		// order of details is C_ID, name, T_ID1, T_ID2, D_ID, Semester
		Msg Add_Course( const std::vector<std::string> & details )
		{
			std::string sql {};

			if ( details[ 3 ] == "NULL" )
			{
				sql =
					"INSERT INTO COURSE (C_ID, Name, T_ID1, D_ID, Semester) "
					"VALUES ( "
					"'" + details[ 0 ] + "', "
					"'" + details[ 1 ] + "', "
					"'" + details[ 2 ] + "', "
					"'" + details[ 4 ] + "', "
					    + details[ 5 ] + "); ";
			}
			else
			{
				sql =
					"INSERT INTO COURSE (C_ID, Name, T_ID1, T_ID2, D_ID, Semester) "
					"VALUES ( "
					"'" + details[ 0 ] + "', "
					"'" + details[ 1 ] + "', "
					"'" + details[ 2 ] + "', "
					"'" + details[ 3 ] + "', "
					"'" + details[ 4 ] + "', "
					    + details[ 5 ] + "); ";
			}
			char * messageError {};
			int exit = sqlite3_exec( m_DB , sql.c_str() , nullptr , nullptr , &messageError );

			Msg msg {};

			if ( exit != SQLITE_OK )
			{
				String str {};
				strcpy_s( str , messageError );
				sqlite3_free( messageError );

				msg.header.id = AppMsg::Result_Err;
				msg << str;
				return msg;
			}
			else
			{
				msg.header.id = AppMsg::Result_Ok;
				return msg;
			}
		}

		Msg View_Course( const std::string & cid )
		{
			std::string sql =
				"SELECT EXISTS ( "
				"SELECT 1 FROM COURSE "
				"WHERE C_ID = '" + cid + "');";

			auto vec1 = exec<1>( sql );

			Msg msg {};

			if ( vec1[ 0 ][ 0 ][ 0 ] == '0' )
			{
				msg.header.id = AppMsg::Result_Err;
				return msg;
			}

			sql =
				"SELECT * FROM COURSE "
				"WHERE C_ID = '" + cid + "';";

			auto vec = exec<6>( sql );

			msg.header.id = AppMsg::Result_Ok;

			for ( int32_t i = ( int32_t ) vec[ 0 ].size() - 1; i >= 0; --i )
			{
				msg << vec[ 0 ][ i ];
			}

			return msg;
		}

		// order of details is C_ID, name, T_ID1, T_ID2, D_ID, Semester
		Msg Edit_Course(
			const std::string & cid ,
			const std::vector<std::string> & details )
		{
			std::string sql =
				"UPDATE COURSE SET "
				"C_ID     = '" + details[ 0 ] + "', "
				"Name     = '" + details[ 1 ] + "', "
				"T_ID1    = '" + details[ 2 ] + "', "
				"T_ID2    = '" + details[ 3 ] + "', "
				"D_ID     = '" + details[ 4 ] + "', "
				"Semester =  " + details[ 5 ] + "   "
				"WHERE C_ID = '" + cid + "';";

			char * messageError {};
			int exit = sqlite3_exec( m_DB , sql.c_str() , nullptr , nullptr , &messageError );

			Msg msg {};

			if ( exit != SQLITE_OK )
			{
				String str {};
				strcpy_s( str , messageError );
				sqlite3_free( messageError );

				msg.header.id = AppMsg::Result_Err;
				msg << str;
				return msg;
			}
			else
			{
				msg.header.id = AppMsg::Result_Ok;
				return msg;
			}
		}

		Msg Del_Course( const std::string & cid )
		{
			std::string sql =
				"DELETE FROM COURSE "
				"WHERE C_ID = '" + cid + "';";

			int exit = sqlite3_exec( m_DB , sql.c_str() , nullptr , nullptr , nullptr );

			std::cout << "Successfull!\n";

			Msg msg {};
			msg.header.id = AppMsg::Result_Ok;

			return msg;
		}
	};

	// Client Class
	class Client : public olc::net::client_interface<AppMsg>
	{
	public:
		Msg Receive()
		{
			while ( IsConnected() && Incoming().empty() )
			{
				std::cout << "Waiting...\n";
			}

			if ( IsConnected() )
			{
				std::cout << "Message Received!\n";
				return Incoming().pop_front().msg;
			}
			else
			{
				std::cout << "Server Down!\n";
				Msg msg;
				msg.header.id = AppMsg::Server_Down;
				return msg;
			}
		}

		AppMsg Login( Entity e , const std::string & id , const std::string & pass )
		{
			String str {};
			Msg msg {};
			msg.header.id = AppMsg::Login;

			msg << e;
			strcpy_s( str , id.c_str() );
			msg << str;
			strcpy_s( str , pass.c_str() );
			msg << str;

			Send( msg );

			return Receive().header.id;
		}

		std::vector<std::string> Course( Entity entity , const std::string & id )
		{
			String str {};
			Msg msg {};
			msg.header.id = AppMsg::Course;

			msg << entity;
			strcpy_s( str , id.c_str() );
			msg << str;

			Send( msg );

			msg = Receive();

			size_t nRows {};
			msg >> nRows;

			std::vector<std::string> table { nRows };

			for ( auto && c : table )
			{
				msg >> str;
				c = str;
			}

			return table;
		}

		AppMsg Change_Pass(
			Entity entity ,
			const std::string & id ,
			const std::string & old ,
			const std::string & pass )
		{
			String str {};
			Msg msg {};
			msg.header.id = AppMsg::Change_Pass;

			msg << entity;
			strcpy_s( str , id.c_str() );
			msg << str;
			strcpy_s( str , old.c_str() );
			msg << str;
			strcpy_s( str , pass.c_str() );
			msg << str;

			Send( msg );

			return Receive().header.id;
		}

		AppMsg Give_Att( const std::string & id , const std::string & cid , int32_t code )
		{
			String str {};
			Msg msg {};
			msg.header.id = AppMsg::Give_Att;

			strcpy_s( str , id.c_str() );
			msg << str;
			strcpy_s( str , cid.c_str() );
			msg << str;
			msg << code;

			Send( msg );

			return Receive().header.id;
		}

		Table<2> Stud_Stats( const std::string & id , const std::string & cid )
		{
			String str {};
			Msg msg {};
			msg.header.id = AppMsg::Stud_Stats;

			strcpy_s( str , id.c_str() );
			msg << str;
			strcpy_s( str , cid.c_str() );
			msg << str;

			Send( msg );

			msg = Receive();
			Table<2> table {};
			size_t nRows {};

			msg >> nRows;

			for ( size_t i = 0; i < nRows; ++i )
			{
				table.emplace_back();
				msg >> table.back();
			}

			return table;
		}

		int32_t Take_Att( const std::string & cid )
		{
			String str {};
			Msg msg {};
			msg.header.id = AppMsg::Take_Att;

			strcpy_s( str , cid.c_str() );
			msg << str;

			Send( msg );

			msg = Receive();
			int32_t code {};
			msg >> code;

			return code;
		}

		AppMsg Stop_Att( const std::string & cid )
		{
			String str {};
			Msg msg {};
			msg.header.id = AppMsg::Stop_Att;

			strcpy_s( str , cid.c_str() );
			msg << str;

			Send( msg );

			return Receive().header.id;
		}

		Table<3> Prof_Stats( const std::string & cid )
		{
			String str {};
			Msg msg {};
			msg.header.id = AppMsg::Prof_Stats;

			strcpy_s( str , cid.c_str() );
			msg << str;

			Send( msg );

			msg = Receive();
			Table<3> table {};
			size_t nRows {};

			msg >> nRows;

			for ( size_t i = 0; i < nRows; ++i )
			{
				table.emplace_back();
				msg >> table.back();
			}

			return table;
		}

		Table<3> Proxy( const std::string & cid )
		{
			String str {};
			Msg msg {};
			msg.header.id = AppMsg::Proxy;

			strcpy_s( str , cid.c_str() );
			msg << str;

			Send( msg );

			msg = Receive();
			Table<3> table {};
			size_t nRows {};

			msg >> nRows;

			for ( size_t i = 0; i < nRows; ++i )
			{
				table.emplace_back();
				msg >> table.back();
			}

			return table;
		}

		Msg Add_Course( const std::vector<std::string> & details )
		{
			String str {};
			Msg msg {};
			msg.header.id = AppMsg::Add_Course;

			for ( auto && row : details )
			{
				strcpy_s( str , row.c_str() );
				msg << str;
			}

			Send( msg );

			return Receive();
		}

		std::vector<std::string> View_Course( const std::string & cid )
		{
			String str {};
			Msg msg {};
			msg.header.id = AppMsg::View_Course;

			strcpy_s( str , cid.c_str() );
			msg << str;

			Send( msg );

			msg = Receive();

			std::vector<std::string> table { 6 };

			if ( msg.header.id != AppMsg::Result_Err )
			{
				for ( auto && val : table )
				{
					msg >> str;
					val = str;
				}				
			}

			return table;
		}

		Msg Edit_Course( const std::string & cid , const std::vector<std::string> & details )
		{
			String str {};
			Msg msg {};
			msg.header.id = AppMsg::Edit_Course;

			strcpy_s( str , cid.c_str() );
			msg << str;

			for ( auto && row : details )
			{
				strcpy_s( str , row.c_str() );

				msg << str;
			}

			Send( msg );

			return Receive();
		}

		AppMsg Del_Course( const std::string & cid )
		{
			String str {};
			Msg msg {};
			msg.header.id = AppMsg::Del_Course;

			strcpy_s( str , cid.c_str() );
			msg << str;

			Send( msg );

			return Receive().header.id;
		}
	};
}
