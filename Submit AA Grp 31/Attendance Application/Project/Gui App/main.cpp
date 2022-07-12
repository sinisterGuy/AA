#include "extra functions.hpp"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <iostream>
#include <tchar.h>

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

struct FrameContext
{
	ID3D12CommandAllocator * CommandAllocator;
	UINT64                  FenceValue;
};

// Data
static int const                    NUM_FRAMES_IN_FLIGHT = 3;
static FrameContext                 g_frameContext[ NUM_FRAMES_IN_FLIGHT ] = {};
static UINT                         g_frameIndex = 0;

static int const                    NUM_BACK_BUFFERS = 3;
static ID3D12Device * g_pd3dDevice = NULL;
static ID3D12DescriptorHeap * g_pd3dRtvDescHeap = NULL;
static ID3D12DescriptorHeap * g_pd3dSrvDescHeap = NULL;
static ID3D12CommandQueue * g_pd3dCommandQueue = NULL;
static ID3D12GraphicsCommandList * g_pd3dCommandList = NULL;
static ID3D12Fence * g_fence = NULL;
static HANDLE                       g_fenceEvent = NULL;
static UINT64                       g_fenceLastSignaledValue = 0;
static IDXGISwapChain3 * g_pSwapChain = NULL;
static HANDLE                       g_hSwapChainWaitableObject = NULL;
static ID3D12Resource * g_mainRenderTargetResource[ NUM_BACK_BUFFERS ] = {};
static D3D12_CPU_DESCRIPTOR_HANDLE  g_mainRenderTargetDescriptor[ NUM_BACK_BUFFERS ] = {};

// Forward declarations of helper functions
bool CreateDeviceD3D( HWND hWnd );
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void WaitForLastSubmittedFrame();
FrameContext * WaitForNextFrameResources();
LRESULT WINAPI WndProc( HWND hWnd , UINT msg , WPARAM wParam , LPARAM lParam );

// Main code
INT WinMain( _In_ HINSTANCE ,
			 _In_opt_ HINSTANCE ,
			 _In_ LPSTR ,
			 _In_ int )
//int main( int , char ** )
{
	// Create application window
	ImGui_ImplWin32_EnableDpiAwareness();
	WNDCLASSEX wc = { sizeof( WNDCLASSEX ) , CS_CLASSDC , WndProc , 0L , 0L , GetModuleHandle( NULL ) , NULL , NULL , NULL , NULL , _T( "ImGui Example" ) , NULL };
	::RegisterClassEx( &wc );
	HWND hwnd = ::CreateWindow( wc.lpszClassName , _T( "Attendance Application" ) , WS_OVERLAPPEDWINDOW , 100 , 100 , 1280 , 800 , NULL , NULL , wc.hInstance , NULL );

	// Initialize Direct3D
	if ( !CreateDeviceD3D( hwnd ) )
	{
		CleanupDeviceD3D();
		::UnregisterClass( wc.lpszClassName , wc.hInstance );
		return 1;
	}

	// Show the window
	::ShowWindow( hwnd , SW_SHOWDEFAULT );
	::UpdateWindow( hwnd );

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO & io = ImGui::GetIO(); ( void ) io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
	//io.ConfigViewportsNoAutoMerge = true;
	//io.ConfigViewportsNoTaskBarIcon = true;

	// Setup Dear ImGui style
	//ImGui::StyleColorsDark();
	ImGui::StyleColorsLight();
	//ImGui::StyleColorsClassic();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle & style = ImGui::GetStyle();
	if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
	{
		style.WindowRounding = 0.0f;
		style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
	}

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init( hwnd );
	ImGui_ImplDX12_Init( g_pd3dDevice , NUM_FRAMES_IN_FLIGHT ,
						 DXGI_FORMAT_R8G8B8A8_UNORM , g_pd3dSrvDescHeap ,
						 g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart() ,
						 g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart() );


	io.Fonts->AddFontFromFileTTF( "./fonts/tahoma.ttf" , 25.0f );

	// Our state
	bool show_demo_window = true;
	bool show_another_window = false;
	ImVec4 clear_color = ImVec4( 0.45f , 0.55f , 0.60f , 1.00f );

	// Establish the connection
	if ( !App::client.Connect( "127.0.0.1" , 60000 ) )
	{
		return 1;
	}
	else
	{
		App::AppMsg msg = App::client.Receive().header.id;

		if ( msg == App::AppMsg::ServerAccept )
		{
			std::cout << "Server Accepted Connection!\n";
		}
		else
		{
			return 1;
		}
	}


	std::string id {};
	std::vector<std::string> courses {};
	App::Entity entity = App::Entity::None;
	bool login_window = true , change_pass = false; // for all user
	bool stud_login = false , prof_login = false , admin_login = false;  // login as user
	bool stud_pane = false , prof_pane = false , admin_pane = false;  // panes for user
	bool give_att = false , stats_course = false;  // options : students
	bool take_att = false , stats_att = false , proxy = false;  // options : teacher
	bool add_course = false , view_course = false , edit_course = false , del_course = false;  // options: admin

	ImGuiWindowFlags win_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;

	// Main loop
	bool done = false;
	while ( !done )
	{
		MSG msg;
		while ( ::PeekMessage( &msg , NULL , 0U , 0U , PM_REMOVE ) )
		{
			::TranslateMessage( &msg );
			::DispatchMessage( &msg );

			if ( msg.message == WM_QUIT )
			{
				done = true;
			}
		}

		if ( done ) { break; }

		// Start the Dear ImGui frame
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::DockSpaceOverViewport( ImGui::GetMainViewport() );
		ImGui::GetStyle().Colors[ ImGuiCol_Text ] = ImVec4( 0.0f , 0.31f , 0.56f , 1.0f );

		// login as window		
		if ( login_window )
		{
			ImGui::Begin( "Login" , &login_window , win_flags );
			done = !login_window;

			App::CenterText( "Login As :" );
			if ( App::CenterButton( "Student" , { 150 , 50 } ) )
			{
				stud_login = true;
				login_window = false;
			}

			if ( App::CenterButton( "Professor" , { 150 , 50 } ) )
			{
				prof_login = true;
				login_window = false;
			}

			if ( App::CenterButton( "Admin" , { 150 , 50 } ) )
			{
				admin_login = true;
				login_window = false;
			}

			ImGui::End();
		}

		// student
		if ( stud_login )
		{
			ImGui::Begin( "Student" , &stud_login , win_flags );
			login_window = !stud_login;
			App::CenterText( "Student" );

			static int ok = 0;
			if ( ok == 0 )
			{
				ok = App::login( App::Entity::Student , id );
			}

			if ( ok == 1 )
			{
				ImGui::OpenPopup( "Success!" );
				ImGui::BeginPopupModal( "Success!" , nullptr , ImGuiWindowFlags_NoResize );

				ImGui::Text( "Successful Login!" );
				ImGui::Separator();

				if ( App::CenterButton( "OK" , ImVec2( 120 , 0 ) ) )
				{
					ImGui::CloseCurrentPopup();
					ok = 0;
					stud_login = false;
					stud_pane = true;
					courses = App::get_courses( App::Entity::Student , id );
					entity = App::Entity::Student;
				}
				ImGui::SetItemDefaultFocus();

				ImGui::EndPopup();
			}
			else if ( ok == -1 )
			{
				ImGui::OpenPopup( "Error...!" );
				ImGui::BeginPopupModal( "Error...!" , nullptr , ImGuiWindowFlags_NoResize );

				ImGui::Text( "Invalid Login Credentials!" );
				ImGui::Separator();

				if ( App::CenterButton( "OK" , ImVec2( 120 , 0 ) ) )
				{
					ImGui::CloseCurrentPopup();
					ok = 0;
				}
				ImGui::SetItemDefaultFocus();

				ImGui::EndPopup();
			}

			ImGui::End();
		}

		// professor
		if ( prof_login )
		{
			ImGui::Begin( "Teacher" , &prof_login , win_flags );
			login_window = !prof_login;
			App::CenterText( "Teacher" );

			static int ok = 0;
			if ( ok == 0 )
			{
				ok = App::login( App::Entity::Professor , id );
			}

			if ( ok == 1 )
			{
				ImGui::OpenPopup( "Success!" );
				ImGui::BeginPopupModal( "Success!" , nullptr , ImGuiWindowFlags_NoResize );

				ImGui::Text( "Successful Login!" );
				ImGui::Separator();

				if ( App::CenterButton( "OK" , ImVec2( 120 , 0 ) ) )
				{
					ImGui::CloseCurrentPopup();
					ok = 0;
					prof_login = false;
					prof_pane = true;
					courses = App::get_courses( App::Entity::Professor , id );
					entity = App::Entity::Professor;
				}
				ImGui::SetItemDefaultFocus();

				ImGui::EndPopup();
			}
			else if ( ok == -1 )
			{
				ImGui::OpenPopup( "Error...!" );
				ImGui::BeginPopupModal( "Error...!" , nullptr , ImGuiWindowFlags_NoResize );

				ImGui::Text( "Invalid Login Credentials!" );
				ImGui::Separator();

				if ( App::CenterButton( "OK" , ImVec2( 120 , 0 ) ) )
				{
					ImGui::CloseCurrentPopup();
					ok = 0;
				}
				ImGui::SetItemDefaultFocus();

				ImGui::EndPopup();
			}

			ImGui::End();
		}

		// admin
		if ( admin_login )
		{
			ImGui::Begin( "Admin" , &admin_login , win_flags );
			login_window = !admin_login;
			App::CenterText( "Admin" );

			static int ok = 0;
			if ( ok == 0 )
			{
				ok = App::login( App::Entity::Admin , id );
			}

			if ( ok == 1 )
			{
				ImGui::OpenPopup( "Success!" );
				ImGui::BeginPopupModal( "Success!" , nullptr , ImGuiWindowFlags_NoResize );

				ImGui::Text( "Successful Login!" );
				ImGui::Separator();

				if ( App::CenterButton( "OK" , ImVec2( 120 , 0 ) ) )
				{
					ImGui::CloseCurrentPopup();
					ok = 0;
					admin_login = false;
					admin_pane = true;
					entity = App::Entity::Admin;
				}
				ImGui::SetItemDefaultFocus();

				ImGui::EndPopup();
			}
			else if ( ok == -1 )
			{
				ImGui::OpenPopup( "Error...!" );
				ImGui::BeginPopupModal( "Error...!" , nullptr , ImGuiWindowFlags_NoResize );

				ImGui::Text( "Invalid Login Credentials!" );
				ImGui::Separator();

				if ( App::CenterButton( "OK" , ImVec2( 120 , 0 ) ) )
				{
					ImGui::CloseCurrentPopup();
					ok = 0;
				}
				ImGui::SetItemDefaultFocus();

				ImGui::EndPopup();
			}

			ImGui::End();
		}

		// Student Activities

		if ( stud_pane )
		{
			ImGui::Begin( "Student" , &stud_pane , win_flags );
			login_window = !stud_pane;
			if ( !stud_pane )
			{
				courses.clear();
				entity = App::Entity::None;
			}

			if ( App::CenterButton( "Give Attendance" , { 200 , 50 } ) )
			{
				stud_pane = false;
				give_att = true;
			}

			if ( App::CenterButton( "Get Statistics" , { 200 , 50 } ) )
			{
				stud_pane = false;
				stats_course = true;
			}

			if ( App::CenterButton( "Change Password" , { 200 , 50 } ) )
			{
				stud_pane = false;
				change_pass = true;
			}

			ImGui::End();
		}

		if ( give_att )
		{
			ImGui::Begin( "Give Attendance" , &give_att , win_flags );
			stud_pane = !give_att;
			static std::string current_item {} , code {};
			static bool pop1 {} , pop2 {} , pop {};

			if ( ImGui::BeginCombo( "Select Course" , current_item.c_str() ) )
			{
				for ( auto && n : courses )
				{
					bool is_selected = ( current_item == n );
					if ( ImGui::Selectable( n.c_str() , is_selected ) )
						current_item = n;
				}
				ImGui::EndCombo();
			}

			ImGui::InputText( "Enter Magic Code" , &code ,
							  ImGuiInputTextFlags_CharsNoBlank |
							  ImGuiInputTextFlags_CallbackCharFilter ,
							  App::FilterLetters );

			if ( App::CenterButton( "Submit" , { 120 , 50 } ) )
			{
				if ( current_item.empty() || code.empty() )
				{
					pop = true;
				}
				else
				{
					pop1 = App::register_Att( id , current_item , code );
					pop2 = !pop1;
				}
			}

			if ( pop1 )
			{
				ImGui::OpenPopup( "Done" );
				ImGui::BeginPopupModal( "Done" );

				ImGui::TextUnformatted( "Attendance Registered!" );

				if ( App::CenterButton( "OK" , { 100 , 50 } ) )
				{
					pop1 = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if ( pop2 )
			{
				ImGui::OpenPopup( "Error" );
				ImGui::BeginPopupModal( "Error" );

				ImGui::TextUnformatted( "Could Not Register Attendance!" );

				if ( App::CenterButton( "OK" , { 100 , 50 } ) )
				{
					pop2 = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if ( pop )
			{
				ImGui::OpenPopup( "Error" );
				ImGui::BeginPopupModal( "Error" );

				ImGui::TextUnformatted( "No field can be blank!" );

				if ( App::CenterButton( "OK" , { 100 , 50 } ) )
				{
					pop = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if ( !give_att )
			{
				current_item.clear();
				code.clear();
			}

			ImGui::End();
		}

		if ( stats_course )
		{
			ImGui::Begin( "Statistics" , &stats_course , win_flags );
			stud_pane = !stats_course;

			static bool display = false , pop {};
			static std::string current_item {};
			static std::vector<std::vector<std::string>> stats {} , stats_rev {};

			if ( ImGui::BeginCombo( "Select Course" , current_item.c_str() ) )
			{
				for ( auto && n : courses )
				{
					bool is_selected = ( current_item == n );
					if ( ImGui::Selectable( n.c_str() , is_selected ) )
						current_item = n;
				}

				display = false;
				ImGui::EndCombo();
			}

			if ( App::CenterButton( "Submit" , { 120 , 50 } ) )
			{
				if ( !current_item.empty() )
				{
					stats = App::get_Stats( App::Entity::Student , id , current_item );
					stats_rev = std::vector<std::vector<std::string>> { stats.rbegin() , stats.rend() };
					display = true;
				}
				else
				{
					pop = true;
				}
			}

			if ( display )
			{
				App::CenterText( "Statistics for " + current_item );
				ImGui::BeginTable( "table1" , 2 ,
								   ImGuiTableFlags_RowBg |
								   ImGuiTableFlags_Borders );

				ImGui::TableSetupColumn( "Date" );
				ImGui::TableSetupColumn( "Present/Absent" );
				ImGui::TableHeadersRow();

				for ( auto && row : stats )
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted( row[ 0 ].c_str() );
					ImGui::TableNextColumn();
					ImGui::TextUnformatted( row[ 1 ].c_str() );
				}

				ImGui::EndTable();

				if ( App::CenterButton( "Reverse" , { 120 , 50 } ) )
				{
					std::swap( stats , stats_rev );
				}
			}

			if ( pop )
			{
				ImGui::OpenPopup( "Error" );
				ImGui::BeginPopupModal( "Error" );

				ImGui::TextUnformatted( "Select Course ID!" );

				if ( App::CenterButton( "OK" , { 120 , 50 } ) )
				{
					pop = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if ( !stats_course )
			{
				current_item.clear();
				display = false;
			}

			ImGui::End();
		}

		// Teacher Activities

		if ( prof_pane )
		{
			ImGui::Begin( "Teacher" , &prof_pane , win_flags );
			login_window = !prof_pane;
			if ( !prof_pane )
			{
				courses.clear();
				entity = App::Entity::None;
			}

			if ( App::CenterButton( "Take Attendance" , { 200 , 50 } ) )
			{
				prof_pane = false;
				take_att = true;
			}

			if ( App::CenterButton( "Get Statistics" , { 200 , 50 } ) )
			{
				prof_pane = false;
				stats_att = true;
			}

			if ( App::CenterButton( "Get Proxies" , { 200 , 50 } ) )
			{
				prof_login = false;
				proxy = true;
			}

			if ( App::CenterButton( "Change Password" , { 200 , 50 } ) )
			{
				prof_pane = false;
				change_pass = true;
			}


			ImGui::End();
		}

		if ( take_att )
		{
			ImGui::Begin( "Take Attendance" , &take_att , win_flags );
			prof_pane = !take_att;
			static std::string current_item {};
			static bool att {};
			static int32_t code {};

			if ( ImGui::BeginCombo( "Select Course" , current_item.c_str() ) )
			{
				for ( auto && n : courses )
				{
					bool is_selected = ( current_item == n );
					if ( ImGui::Selectable( n.c_str() , is_selected ) )
						current_item = n;
				}
				ImGui::EndCombo();
			}

			if ( App::CenterButton( "Start Attendance" , { 200 , 50 } ) )
			{
				code = App::Start_Att( current_item );
				att = true;
			}

			if ( att )
			{
				App::CenterText( "Magic Code is " + std::to_string( code ) );

				if ( App::CenterButton( "Stop Attendance" , { 200 , 50 } ) )
				{
					App::Stop_Att( current_item );
					att = false;
				}
			}

			if ( !take_att )
			{
				current_item.clear();
			}

			ImGui::End();
		}

		if ( stats_att )
		{
			ImGui::Begin( "Statistics" , &stats_att , win_flags );
			prof_pane = !stats_att;

			static bool display = false;
			static std::string current_item {};
			static std::vector<std::vector<std::string>> stats {} , stats_rev {};

			if ( ImGui::BeginCombo( "Select Course" , current_item.c_str() ) )
			{
				for ( auto && n : courses )
				{
					bool is_selected = ( current_item == n );
					if ( ImGui::Selectable( n.c_str() , is_selected ) )
						current_item = n;
				}

				display = false;
				ImGui::EndCombo();
			}

			if ( App::CenterButton( "Submit" , { 120 , 50 } ) )
			{
				stats = App::get_Stats( App::Entity::Professor , id , current_item );
				stats_rev = std::vector<std::vector<std::string>> { stats.rbegin() , stats.rend() };
				display = true;
			}

			if ( display )
			{
				App::CenterText( ( "Statistics for " + current_item ).c_str() );
				ImGui::BeginTable( "table1" , 3 ,
								   ImGuiTableFlags_RowBg |
								   ImGuiTableFlags_Borders );

				ImGui::TableSetupColumn( "Date" );
				ImGui::TableSetupColumn( "Student ID" );
				ImGui::TableSetupColumn( "Present/Absent" );
				ImGui::TableHeadersRow();

				for ( auto && row : stats )
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted( row[ 0 ].c_str() );
					ImGui::TableNextColumn();
					ImGui::TextUnformatted( row[ 1 ].c_str() );
					ImGui::TableNextColumn();
					ImGui::TextUnformatted( row[ 2 ].c_str() );
				}

				ImGui::EndTable();

				if ( App::CenterButton( "Reverse" , { 120 , 50 } ) )
				{
					std::swap( stats , stats_rev );
				}
			}


			if ( !stats_att )
			{
				current_item.clear();
				stats.clear();
				display = false;
			}

			ImGui::End();
		}

		if ( proxy )
		{
			ImGui::Begin( "Proxies" , &proxy , win_flags );
			prof_pane = !proxy;

			static bool display = false;
			static std::string current_item {};
			static std::vector<std::vector<std::string>> stats {};

			if ( ImGui::BeginCombo( "Select Course" , current_item.c_str() ) )
			{
				for ( auto && n : courses )
				{
					bool is_selected = ( current_item == n );
					if ( ImGui::Selectable( n.c_str() , is_selected ) )
						current_item = n;
				}

				display = false;
				ImGui::EndCombo();
			}

			if ( App::CenterButton( "Submit" , { 120 , 50 } ) )
			{
				stats = App::get_Proxy( current_item );
				display = true;
			}

			if ( display )
			{
				App::CenterText( ( "Statistics for " + current_item ).c_str() );
				ImGui::BeginTable( "table1" , 3 ,
								   ImGuiTableFlags_RowBg |
								   ImGuiTableFlags_Borders );

				ImGui::TableSetupColumn( "Date" );
				ImGui::TableSetupColumn( "Student ID" );
				ImGui::TableSetupColumn( "IP Address" );
				ImGui::TableHeadersRow();

				for ( auto && row : stats )
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted( row[ 0 ].c_str() );
					ImGui::TableNextColumn();
					ImGui::TextUnformatted( row[ 1 ].c_str() );
					ImGui::TableNextColumn();
					ImGui::TextUnformatted( row[ 2 ].c_str() );
				}

				ImGui::EndTable();
			}

			if ( !proxy )
			{
				current_item.clear();
				stats.clear();
				display = false;
			}

			ImGui::End();
		}

		// Admin Activities

		if ( admin_pane )
		{
			ImGui::Begin( "Admin" , &admin_pane , win_flags );
			login_window = !admin_pane;
			if ( !admin_pane )
			{
				entity = App::Entity::None;
			}

			if ( App::CenterButton( "Add Courses" , { 150 , 50 } ) )
			{
				admin_pane = false;
				add_course = true;
			}

			if ( App::CenterButton( "View Courses" , { 150 , 50 } ) )
			{
				admin_pane = false;
				view_course = true;
			}

			if ( App::CenterButton( "Change Password" , { 200 , 50 } ) )
			{
				admin_pane = false;
				change_pass = true;
			}

			ImGui::End();
		}

		if ( add_course )
		{
			ImGui::Begin( "Add Course" , &add_course , win_flags );
			admin_pane = !add_course;

			static bool pop {} , pop1 {};
			static std::string id {} , dep {} , name {} , T_id1 {} , T_id2 {} , sem {} , errMsg {};

			ImGui::InputText( "Enter Course ID" , &id ,
							  ImGuiInputTextFlags_CharsNoBlank );

			ImGui::InputText( "Enter Name of Course" , &name );

			ImGui::InputText( "Enter Department ID" , &dep ,
							  ImGuiInputTextFlags_CharsNoBlank );

			ImGui::InputText( "Enter Semester" , &sem ,
							  ImGuiInputTextFlags_CharsNoBlank );

			ImGui::InputText( "Enter Teacher 1 ID" , &T_id1 ,
							  ImGuiInputTextFlags_CharsNoBlank );

			ImGui::InputText( "Enter Teacher 2 ID (Write NA if None)" , &T_id2 ,
							  ImGuiInputTextFlags_CharsNoBlank );

			if ( App::CenterButton( "Submit" , { 120 , 50 } ) )
			{
				pop = App::create_course( { id , name , T_id1 ,
										  ( _strcmpi( T_id2.c_str() , "NA" ) == 0 ? "NULL" : T_id2 ) ,
										  dep , sem } , errMsg );
				pop1 = !pop;
				id.clear() , dep.clear() , name.clear() , sem.clear() , T_id1.clear() , T_id2.clear();
			}

			if ( pop )
			{
				ImGui::OpenPopup( "Done" );
				ImGui::BeginPopupModal( "Done" );

				if ( App::CenterButton( "OK" , { 100 , 50 } ) )
				{
					pop = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if ( pop1 )
			{
				ImGui::OpenPopup( "Error" );
				ImGui::BeginPopupModal( "Error" );

				ImGui::TextUnformatted( ( errMsg + " Try Again..." ).c_str() );

				if ( App::CenterButton( "OK" , { 120 , 50 } ) )
				{
					pop1 = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			ImGui::End();
		}

		if ( view_course )
		{
			ImGui::Begin( "View Course" , &view_course , win_flags );
			admin_pane = !view_course;
			static std::string id {} , errMsg {};
			static std::vector<std::string> details {} , details1 {};
			static bool display {} , pop {} , pop1 {};

			ImGui::InputText( "Enter Course ID" , &id ,
							  ImGuiInputTextFlags_CharsNoBlank );

			if ( App::CenterButton( "View" , { 120 , 50 } ) )
			{
				details = App::get_details( id );
				display = !details[ 0 ].empty();
				if ( !display )
				{
					pop1 = true;
					errMsg = "Wrong Course ID!";
				}
			}

			if ( display )
			{
				ImGui::BeginTable( "table2" , 2 ,
								   ImGuiTableFlags_RowBg |
								   ImGuiTableFlags_Borders );

				ImGui::TableNextColumn();
				ImGui::TextUnformatted( "Course ID" );
				ImGui::TableNextColumn();
				if ( edit_course )
					ImGui::InputText( "##id" , &details1[ 0 ] , ImGuiInputTextFlags_CharsNoBlank );
				else ImGui::TextUnformatted( details[ 0 ].empty() ? "NA" : details[ 0 ].c_str() );

				ImGui::TableNextColumn();
				ImGui::TextUnformatted( "Course Name" );
				ImGui::TableNextColumn();
				if ( edit_course )
					ImGui::InputText( "##name" , &details1[ 1 ] , ImGuiInputTextFlags_CharsNoBlank );
				else ImGui::TextUnformatted( details[ 1 ].empty() ? "NA" : details[ 1 ].c_str() );

				ImGui::TableNextColumn();
				ImGui::TextUnformatted( "Teacher 1 ID" );
				ImGui::TableNextColumn();
				if ( edit_course )
					ImGui::InputText( "##t1" , &details1[ 2 ] , ImGuiInputTextFlags_CharsNoBlank );
				else ImGui::TextUnformatted( details[ 2 ].empty() ? "NA" : details[ 2 ].c_str() );

				ImGui::TableNextColumn();
				ImGui::TextUnformatted( "Teacher 2 ID" );
				ImGui::TableNextColumn();
				if ( edit_course )
					ImGui::InputText( "Enter NA if None" , &details1[ 3 ] , ImGuiInputTextFlags_CharsNoBlank );
				else ImGui::TextUnformatted( details[ 3 ].empty() ? "NA" : details[ 3 ].c_str() );

				ImGui::TableNextColumn();
				ImGui::TextUnformatted( "Department" );
				ImGui::TableNextColumn();
				if ( edit_course )
					ImGui::InputText( "##dep" , &details1[ 4 ] , ImGuiInputTextFlags_CharsNoBlank );
				else ImGui::TextUnformatted( details[ 4 ].empty() ? "NA" : details[ 4 ].c_str() );

				ImGui::TableNextColumn();
				ImGui::TextUnformatted( "Course Sem" );
				ImGui::TableNextColumn();
				if ( edit_course )
					ImGui::InputText( "##sem" , &details1[ 5 ] , ImGuiInputTextFlags_CharsNoBlank );
				else ImGui::TextUnformatted( details[ 5 ].empty() ? "NA" : details[ 5 ].c_str() );

				ImGui::EndTable();

				if ( !edit_course && App::CenterButton( "Edit" , { 120 , 50 } ) )
				{
					edit_course = true;
					details1 = details;
				}

				if ( edit_course )
				{
					if ( App::CenterButton( "Submit" , { 120 , 50 } ) )
					{
						if ( _strcmpi( details1[ 3 ].c_str() , "NA" ) == 0 )
						{
							details1[ 3 ] = "NULL";
						}

						pop1 = !App::Edit_Course( details[ 0 ] , details1 , errMsg );
						id.clear();
						details.clear();
						display = false;
						pop = !pop1;
						edit_course = false;
					}

					if ( App::CenterButton( "Back" , { 120 , 50 } ) )
					{
						edit_course = false;
					}
				}

				else if ( App::CenterButton( "Delete" , { 120 , 50 } ) )
				{
					del_course = true;
				}

				if ( del_course )
				{
					ImGui::OpenPopup( "Confirm" );
					ImGui::BeginPopupModal( "Confirm" );

					ImGui::Text( "Delete course %s ?" , details[ 0 ].c_str() );

					if ( App::CenterButton( "Yes" , { 100 , 50 } ) )
					{
						App::Del_Course( details[ 0 ] );
						id.clear();
						details.clear();
						pop = true;
						display = false;
						del_course = false;
						ImGui::CloseCurrentPopup();
					}

					if ( App::CenterButton( "No" , { 100 , 50 } ) )
					{
						del_course = false;
						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();

				}

				if ( !view_course )
				{
					display = false;
					id.clear();
					details.clear();
					details1.clear();
				}
			}

			if ( pop )
			{
				ImGui::OpenPopup( "Done" );
				ImGui::BeginPopupModal( "Done" );

				if ( App::CenterButton( "OK" , { 100 , 50 } ) )
				{
					pop = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if ( pop1 )
			{
				ImGui::OpenPopup( "Error" );
				ImGui::BeginPopupModal( "Error" );

				ImGui::TextUnformatted( errMsg.c_str() );

				if ( App::CenterButton( "OK" , { 100 , 50 } ) )
				{
					pop1 = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			ImGui::End();
		}


		if ( change_pass )
		{
			ImGui::Begin( "Change Password" , &change_pass , win_flags );
			switch ( entity )
			{
				case App::Entity::Student:
					stud_pane = !change_pass;
					break;
				case App::Entity::Professor:
					prof_pane = !change_pass;
					break;
				case App::Entity::Admin:
					admin_pane = !change_pass;
					break;
				default:
					break;
			}

			static std::string old_pass {} , new_pass {} , rep_pass {};
			static bool pop1 = false , pop2 = false , pop3 = false , pop4 = false;

			/*ImGui::InputText( "Enter Old Password" , &old_pass );
			ImGui::InputText( "Enter New Password" , &new_pass );
			ImGui::InputText( "Re-enter New Password" , &rep_pass );*/

			ImGui::InputText( "Enter Old Password" , &old_pass ,
							  ImGuiInputTextFlags_Password );
			ImGui::InputText( "Enter New Password" , &new_pass ,
							  ImGuiInputTextFlags_Password );
			ImGui::InputText( "Re-enter New Password" , &rep_pass ,
							  ImGuiInputTextFlags_Password );

			if ( App::CenterButton( "Submit" , { 120 , 50 } ) )
			{
				if ( new_pass.empty() )
				{
					pop1 = true;
				}

				else if ( new_pass != rep_pass )
				{
					pop2 = true;
				}

				else
				{
					pop3 = App::Change_Pass( entity , id , old_pass , new_pass );
					pop4 = !pop3;
				}
			}


			if ( pop1 )
			{
				ImGui::OpenPopup( "Error" );
				ImGui::BeginPopupModal( "Error" );

				ImGui::TextUnformatted( "New Passwords Can't Be Empty! Try Again..." );

				if ( App::CenterButton( "OK" , { 120 , 50 } ) )
				{
					pop1 = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if ( pop2 )
			{
				ImGui::OpenPopup( "Error" );
				ImGui::BeginPopupModal( "Error" );

				ImGui::TextUnformatted( "New Passwords Don't Match! Try Again..." );

				if ( App::CenterButton( "OK" , { 120 , 50 } ) )
				{
					pop2 = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if ( pop3 )
			{
				ImGui::OpenPopup( "Success!" );
				ImGui::BeginPopupModal( "Success!" );

				ImGui::TextUnformatted( "Password Changed Successfully!" );

				if ( App::CenterButton( "OK" , { 120 , 50 } ) )
				{
					pop3 = false;
					change_pass = false;
					login_window = true;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if ( pop4 )
			{
				ImGui::OpenPopup( "Error" );
				ImGui::BeginPopupModal( "Error" );

				ImGui::TextUnformatted( "Wrong Old Password! Try Again..." );

				if ( App::CenterButton( "OK" , { 120 , 50 } ) )
				{
					pop4 = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if ( !change_pass )
			{
				old_pass.clear() , new_pass.clear() , rep_pass.clear();
			}

			ImGui::End();
		}

		// Rendering
		ImGui::Render();

		FrameContext * frameCtx = WaitForNextFrameResources();
		UINT backBufferIdx = g_pSwapChain->GetCurrentBackBufferIndex();
		frameCtx->CommandAllocator->Reset();

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = g_mainRenderTargetResource[ backBufferIdx ];
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		g_pd3dCommandList->Reset( frameCtx->CommandAllocator , NULL );
		g_pd3dCommandList->ResourceBarrier( 1 , &barrier );

		// Render Dear ImGui graphics
		const float clear_color_with_alpha[ 4 ] = { clear_color.x * clear_color.w , clear_color.y * clear_color.w , clear_color.z * clear_color.w , clear_color.w };
		g_pd3dCommandList->ClearRenderTargetView( g_mainRenderTargetDescriptor[ backBufferIdx ] , clear_color_with_alpha , 0 , NULL );
		g_pd3dCommandList->OMSetRenderTargets( 1 , &g_mainRenderTargetDescriptor[ backBufferIdx ] , FALSE , NULL );
		g_pd3dCommandList->SetDescriptorHeaps( 1 , &g_pd3dSrvDescHeap );
		ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData() , g_pd3dCommandList );
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		g_pd3dCommandList->ResourceBarrier( 1 , &barrier );
		g_pd3dCommandList->Close();

		g_pd3dCommandQueue->ExecuteCommandLists( 1 , ( ID3D12CommandList * const * ) &g_pd3dCommandList );

		// Update and Render additional Platform Windows
		if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault( NULL , ( void * ) g_pd3dCommandList );
		}

		g_pSwapChain->Present( 1 , 0 ); // Present with vsync
		//g_pSwapChain->Present(0, 0); // Present without vsync

		UINT64 fenceValue = g_fenceLastSignaledValue + 1;
		g_pd3dCommandQueue->Signal( g_fence , fenceValue );
		g_fenceLastSignaledValue = fenceValue;
		frameCtx->FenceValue = fenceValue;
	}

	App::client.Disconnect();

	WaitForLastSubmittedFrame();

	// Cleanup
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceD3D();
	::DestroyWindow( hwnd );
	::UnregisterClass( wc.lpszClassName , wc.hInstance );

	return 0;
}

// Helper functions

bool CreateDeviceD3D( HWND hWnd )
{
	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC1 sd;
	{
		ZeroMemory( &sd , sizeof( sd ) );
		sd.BufferCount = NUM_BACK_BUFFERS;
		sd.Width = 0;
		sd.Height = 0;
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		sd.Scaling = DXGI_SCALING_STRETCH;
		sd.Stereo = FALSE;
	}

	// [DEBUG] Enable debug interface
#ifdef DX12_ENABLE_DEBUG_LAYER
	ID3D12Debug * pdx12Debug = NULL;
	if ( SUCCEEDED( D3D12GetDebugInterface( IID_PPV_ARGS( &pdx12Debug ) ) ) )
		pdx12Debug->EnableDebugLayer();
#endif

	// Create device
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	if ( D3D12CreateDevice( NULL , featureLevel , IID_PPV_ARGS( &g_pd3dDevice ) ) != S_OK )
		return false;

	// [DEBUG] Setup debug interface to break on any warnings/errors
#ifdef DX12_ENABLE_DEBUG_LAYER
	if ( pdx12Debug != NULL )
	{
		ID3D12InfoQueue * pInfoQueue = NULL;
		g_pd3dDevice->QueryInterface( IID_PPV_ARGS( &pInfoQueue ) );
		pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_ERROR , true );
		pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_CORRUPTION , true );
		pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_WARNING , true );
		pInfoQueue->Release();
		pdx12Debug->Release();
	}
#endif

	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.NumDescriptors = NUM_BACK_BUFFERS;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 1;
		if ( g_pd3dDevice->CreateDescriptorHeap( &desc , IID_PPV_ARGS( &g_pd3dRtvDescHeap ) ) != S_OK )
			return false;

		SIZE_T rtvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
		for ( UINT i = 0; i < NUM_BACK_BUFFERS; i++ )
		{
			g_mainRenderTargetDescriptor[ i ] = rtvHandle;
			rtvHandle.ptr += rtvDescriptorSize;
		}
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 1;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		if ( g_pd3dDevice->CreateDescriptorHeap( &desc , IID_PPV_ARGS( &g_pd3dSrvDescHeap ) ) != S_OK )
			return false;
	}

	{
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 1;
		if ( g_pd3dDevice->CreateCommandQueue( &desc , IID_PPV_ARGS( &g_pd3dCommandQueue ) ) != S_OK )
			return false;
	}

	for ( UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++ )
		if ( g_pd3dDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT , IID_PPV_ARGS( &g_frameContext[ i ].CommandAllocator ) ) != S_OK )
			return false;

	if ( g_pd3dDevice->CreateCommandList( 0 , D3D12_COMMAND_LIST_TYPE_DIRECT , g_frameContext[ 0 ].CommandAllocator , NULL , IID_PPV_ARGS( &g_pd3dCommandList ) ) != S_OK ||
		 g_pd3dCommandList->Close() != S_OK )
		return false;

	if ( g_pd3dDevice->CreateFence( 0 , D3D12_FENCE_FLAG_NONE , IID_PPV_ARGS( &g_fence ) ) != S_OK )
		return false;

	g_fenceEvent = CreateEvent( NULL , FALSE , FALSE , NULL );
	if ( g_fenceEvent == NULL )
		return false;

	{
		IDXGIFactory4 * dxgiFactory = NULL;
		IDXGISwapChain1 * swapChain1 = NULL;
		if ( CreateDXGIFactory1( IID_PPV_ARGS( &dxgiFactory ) ) != S_OK )
			return false;
		if ( dxgiFactory->CreateSwapChainForHwnd( g_pd3dCommandQueue , hWnd , &sd , NULL , NULL , &swapChain1 ) != S_OK )
			return false;
		if ( swapChain1->QueryInterface( IID_PPV_ARGS( &g_pSwapChain ) ) != S_OK )
			return false;
		swapChain1->Release();
		dxgiFactory->Release();
		g_pSwapChain->SetMaximumFrameLatency( NUM_BACK_BUFFERS );
		g_hSwapChainWaitableObject = g_pSwapChain->GetFrameLatencyWaitableObject();
	}

	CreateRenderTarget();
	return true;
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if ( g_pSwapChain ) { g_pSwapChain->SetFullscreenState( false , NULL ); g_pSwapChain->Release(); g_pSwapChain = NULL; }
	if ( g_hSwapChainWaitableObject != NULL ) { CloseHandle( g_hSwapChainWaitableObject ); }
	for ( UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++ )
		if ( g_frameContext[ i ].CommandAllocator ) { g_frameContext[ i ].CommandAllocator->Release(); g_frameContext[ i ].CommandAllocator = NULL; }
	if ( g_pd3dCommandQueue ) { g_pd3dCommandQueue->Release(); g_pd3dCommandQueue = NULL; }
	if ( g_pd3dCommandList ) { g_pd3dCommandList->Release(); g_pd3dCommandList = NULL; }
	if ( g_pd3dRtvDescHeap ) { g_pd3dRtvDescHeap->Release(); g_pd3dRtvDescHeap = NULL; }
	if ( g_pd3dSrvDescHeap ) { g_pd3dSrvDescHeap->Release(); g_pd3dSrvDescHeap = NULL; }
	if ( g_fence ) { g_fence->Release(); g_fence = NULL; }
	if ( g_fenceEvent ) { CloseHandle( g_fenceEvent ); g_fenceEvent = NULL; }
	if ( g_pd3dDevice ) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }

#ifdef DX12_ENABLE_DEBUG_LAYER
	IDXGIDebug1 * pDebug = NULL;
	if ( SUCCEEDED( DXGIGetDebugInterface1( 0 , IID_PPV_ARGS( &pDebug ) ) ) )
	{
		pDebug->ReportLiveObjects( DXGI_DEBUG_ALL , DXGI_DEBUG_RLO_SUMMARY );
		pDebug->Release();
	}
#endif
}

void CreateRenderTarget()
{
	for ( UINT i = 0; i < NUM_BACK_BUFFERS; i++ )
	{
		ID3D12Resource * pBackBuffer = NULL;
		g_pSwapChain->GetBuffer( i , IID_PPV_ARGS( &pBackBuffer ) );
		g_pd3dDevice->CreateRenderTargetView( pBackBuffer , NULL , g_mainRenderTargetDescriptor[ i ] );
		g_mainRenderTargetResource[ i ] = pBackBuffer;
	}
}

void CleanupRenderTarget()
{
	WaitForLastSubmittedFrame();

	for ( UINT i = 0; i < NUM_BACK_BUFFERS; i++ )
		if ( g_mainRenderTargetResource[ i ] ) { g_mainRenderTargetResource[ i ]->Release(); g_mainRenderTargetResource[ i ] = NULL; }
}

void WaitForLastSubmittedFrame()
{
	FrameContext * frameCtx = &g_frameContext[ g_frameIndex % NUM_FRAMES_IN_FLIGHT ];

	UINT64 fenceValue = frameCtx->FenceValue;
	if ( fenceValue == 0 )
		return; // No fence was signaled

	frameCtx->FenceValue = 0;
	if ( g_fence->GetCompletedValue() >= fenceValue )
		return;

	g_fence->SetEventOnCompletion( fenceValue , g_fenceEvent );
	WaitForSingleObject( g_fenceEvent , INFINITE );
}

FrameContext * WaitForNextFrameResources()
{
	UINT nextFrameIndex = g_frameIndex + 1;
	g_frameIndex = nextFrameIndex;

	HANDLE waitableObjects[] = { g_hSwapChainWaitableObject , NULL };
	DWORD numWaitableObjects = 1;

	FrameContext * frameCtx = &g_frameContext[ nextFrameIndex % NUM_FRAMES_IN_FLIGHT ];
	UINT64 fenceValue = frameCtx->FenceValue;
	if ( fenceValue != 0 ) // means no fence was signaled
	{
		frameCtx->FenceValue = 0;
		g_fence->SetEventOnCompletion( fenceValue , g_fenceEvent );
		waitableObjects[ 1 ] = g_fenceEvent;
		numWaitableObjects = 2;
	}

	WaitForMultipleObjects( numWaitableObjects , waitableObjects , TRUE , INFINITE );

	return frameCtx;
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd , UINT msg , WPARAM wParam , LPARAM lParam );

// Win32 message handler
LRESULT WINAPI WndProc( HWND hWnd , UINT msg , WPARAM wParam , LPARAM lParam )
{
	if ( ImGui_ImplWin32_WndProcHandler( hWnd , msg , wParam , lParam ) )
		return true;

	switch ( msg )
	{
		case WM_SIZE:
			if ( g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED )
			{
				WaitForLastSubmittedFrame();
				CleanupRenderTarget();
				HRESULT result = g_pSwapChain->ResizeBuffers( 0 , ( UINT ) LOWORD( lParam ) , ( UINT ) HIWORD( lParam ) , DXGI_FORMAT_UNKNOWN , DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT );
				assert( SUCCEEDED( result ) && "Failed to resize swapchain." );
				CreateRenderTarget();
			}
			return 0;
		case WM_SYSCOMMAND:
			if ( ( wParam & 0xfff0 ) == SC_KEYMENU ) // Disable ALT application menu
				return 0;
			break;
		case WM_DESTROY:
			::PostQuitMessage( 0 );
			return 0;
	}
	return ::DefWindowProc( hWnd , msg , wParam , lParam );
}
