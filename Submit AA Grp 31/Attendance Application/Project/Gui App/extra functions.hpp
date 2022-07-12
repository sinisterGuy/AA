#pragma once

#include "Common.hpp"
#include "imgui.h"
#include "imgui_stdlib.h"

#include <iostream>
#include <string>
#include <vector>

namespace App
{
	Client client {};

	bool CenterButton( const char * name , const ImVec2 & size = ImVec2( 0 , 0 ) )
	{
		if ( size.x == 0 )
		{
			ImGui::SetCursorPosX( ( ImGui::GetWindowWidth() -
									ImGui::CalcTextSize( name ).x ) / 2.0f );
		}
		else
		{
			ImGui::SetCursorPosX( ( ImGui::GetWindowWidth() - size.x ) / 2.0f );
		}

		return ImGui::Button( name , size );
	}

	void CenterText( const std::string & name )
	{
		ImGui::SetCursorPosX( ( ImGui::GetWindowWidth() -
								ImGui::CalcTextSize( name.c_str() ).x ) / 2.0f );
		return ImGui::Text( name.c_str() );
	}

	bool validate( Entity entity , const std::string & id , const std::string & password )
	{
		return App::client.Login( entity , id , password ) == AppMsg::Result_Ok;
	}

	int login( Entity entity , std::string & new_id )
	{
		static std::string id {} , password {};

		// ImGui::SameLine();
		ImGui::InputText( "User ID" , &id ,
						  ImGuiInputTextFlags_CharsUppercase |
						  ImGuiInputTextFlags_CharsNoBlank );

		// ImGui::SameLine();
		ImGui::InputText( "Password" , &password ,
						  ImGuiInputTextFlags_Password );

		ImGui::Separator();

		if ( CenterButton( "Login" ) )
		{
			int ok = ( validate( entity , id , password ) ? 1 : -1 );
			new_id = std::move( id );
			id.clear();
			password.clear();
			return ok;
		}

		return 0;
	}

	bool Change_Pass(
		Entity entity ,
		const std::string & id ,
		const std::string & old ,
		const std::string & pass )
	{
		return client.Change_Pass( entity , id , old , pass ) == AppMsg::Result_Ok;
	}

	std::vector<std::string> get_courses( Entity entity , const std::string & id )
	{
		return client.Course( entity , id );
	}

	static int FilterLetters( ImGuiInputTextCallbackData * data )
	{
		return !( data->EventChar < 256 &&
				  strchr( "0123456789" , ( char ) data->EventChar ) );
	}

	bool register_Att(
		const std::string & id ,
		const std::string & course ,
		const std::string & code )
	{
		return client.Give_Att( id , course , std::stoi( code ) ) == AppMsg::Result_Ok;
	}

	int32_t Start_Att( const std::string & course )
	{
		return client.Take_Att( course );
	}

	void Stop_Att( const std::string & course )
	{
		client.Stop_Att( course );
	}

	std::vector<std::vector<std::string>>
		get_Stats(
			Entity entity ,
			const std::string & id ,
			const std::string & course )
	{
		std::vector<std::vector<std::string>> res {};
		switch ( entity )
		{
			case App::Entity::Student:
			{
				auto table = client.Stud_Stats( id , course );
				for ( auto && row : table )
				{
					res.push_back( { row[ 0 ] , row[ 1 ] } );
				}

				return res;
			}
			break;

			case App::Entity::Professor:
			{
				auto table = client.Prof_Stats( course );
				for ( auto && row : table )
				{
					res.push_back( { row[ 0 ] , row[ 1 ] , row[ 2 ] } );
				}

				return res;
			}
			break;

			default:
				return res;
				break;
		}
	}

	std::vector < std::vector<std::string>>
		get_Proxy( const std::string & course )
	{
		auto table = client.Proxy( course );
		std::vector<std::vector<std::string>> res {};

		for ( auto && row : table )
		{
			res.push_back( { row[ 0 ] , row[ 1 ] , row[ 2 ] } );
		}

		return res;
	}

	bool create_course( const std::vector<std::string> & info , std::string & errMsg )
	{
		Msg msg = client.Add_Course( info );

		if ( msg.header.id == AppMsg::Result_Ok )
		{
			errMsg = "";
			return true;
		}
		else
		{
			String str {};
			msg >> str;
			errMsg = str;

			return false;
		}
	}

	std::vector<std::string>
		get_details( const std::string & id )
	{
		return client.View_Course( id );
	}

	bool Edit_Course(
		const std::string & cid ,
		const std::vector<std::string> & details ,
		std::string & errMsg )
	{
		Msg msg = client.Edit_Course( cid , details );

		if ( msg.header.id == AppMsg::Result_Ok )
		{
			errMsg = "";
			return true;
		}
		else
		{
			String str {};
			msg >> str;
			errMsg = str;

			return false;
		}
	}

	void Del_Course( const std::string & id )
	{
		client.Del_Course( id );
	}
}