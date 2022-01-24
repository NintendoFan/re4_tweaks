#include <iostream>
#include "..\includes\stdafx.h"
#include <d3d9.h>
#include "dllmain.h"
#include "Settings.h"
#include "ConsoleWnd.h"
#include "LAApatch.h"
#include "..\external\imgui\imgui.h"
#include "..\external\imgui\imgui_impl_win32.h"
#include "..\external\imgui\imgui_impl_dx9.h"

LAApatch laa;

bool LAApatch::GameIsLargeAddressAware()
{
	static PBYTE module_base = reinterpret_cast<PBYTE>(GetModuleHandle(nullptr));

	PIMAGE_DOS_HEADER dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(module_base);
	PIMAGE_NT_HEADERS nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(module_base + dos_header->e_lfanew);

	return (nt_headers->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE) == IMAGE_FILE_LARGE_ADDRESS_AWARE;
}

LAADialogState LAA_State = LAADialogState::NotShowing;
int LAA_ErrorNum = 0;
void LAApatch::LAARender()
{
	if (GameIsLargeAddressAware())
	{
		// Exit out in case we needlessly ended up here somehow
		LAA_State = LAADialogState::NotShowing;
		return;
	}

	ImGuiIO& io = ImGui::GetIO();

	if (LAA_State == LAADialogState::Showing)
	{
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(400, 175), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(400, 175), ImVec2(400, 175));
		ImGui::Begin("4GB / Large Address Aware patch missing!");
		ImGui::TextWrapped("Your game executable is missing the 4GB/LAA patch, this will likely cause issues with mods that require increased memory.\n\nDo you want re4_tweaks to patch the game EXE for you? (requires relaunch!)");

		ImGui::Spacing();
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 225);

		if (ImGui::Button("Yes", ImVec2(104, 35)))
		{
			char module_path_array[4096];
			GetModuleFileNameA(GetModuleHandle(nullptr), module_path_array, 4096);

			std::string module_path = module_path_array;
			std::string module_path_new = module_path + ".new";
			std::string module_path_bak = module_path + ".bak";

			if (GetFileAttributesA(module_path_new.c_str()) != 0xFFFFFFFF)
				DeleteFileA(module_path_new.c_str());

			if (GetFileAttributesA(module_path_bak.c_str()) != 0xFFFFFFFF)
				DeleteFileA(module_path_bak.c_str());

			BOOL result_CopyFileA = CopyFileA(module_path.c_str(), module_path_new.c_str(), false);

			FILE* file;
			LAA_ErrorNum = fopen_s(&file, module_path_new.c_str(), "rb+");
			if (LAA_ErrorNum == 0)
			{
				IMAGE_DOS_HEADER dos_header;
				size_t result_dos_header = fread(&dos_header, sizeof(IMAGE_DOS_HEADER), 1, file);
				if (result_dos_header != 1)
				{
					fclose(file);
					LAA_ErrorNum = 1;
				}
				else
				{
					fseek(file, dos_header.e_lfanew, SEEK_SET);

					IMAGE_NT_HEADERS nt_headers;
					size_t result_nt_headers = fread(&nt_headers, sizeof(IMAGE_NT_HEADERS), 1, file);
					if (result_nt_headers != 1)
					{
						fclose(file);
						LAA_ErrorNum = 2;
					}
					else
					{
						nt_headers.FileHeader.Characteristics |= IMAGE_FILE_LARGE_ADDRESS_AWARE;

						fseek(file, dos_header.e_lfanew, SEEK_SET);
						auto wrote = fwrite(&nt_headers, sizeof(IMAGE_NT_HEADERS), 1, file);
						fclose(file);
						if (wrote != 1)
						{
							LAA_ErrorNum = 3;
						}
						else
						{
							BOOL result_moveFile1 = MoveFileA(module_path.c_str(), module_path_bak.c_str());
							BOOL result_moveFile2 = MoveFileA(module_path_new.c_str(), module_path.c_str());
							if (!result_moveFile1)
								LAA_ErrorNum = 4;
							else if (!result_moveFile2)
								LAA_ErrorNum = 5;

							if (result_moveFile1 && !result_moveFile2)
							{
								// Users original EXE was moved, but we couldn't move replacement for it for some reason
								// Try restoring the users original EXE so they aren't left with a broken install...
								MoveFileA(module_path_bak.c_str(), module_path.c_str());
							}
						}
					}
				}
			}
			LAA_State = LAADialogState::Finished;
		}

		ImGui::SameLine();

		if (ImGui::Button("No", ImVec2(104, 35)))
		{
			LAA_State = LAADialogState::NotShowing;
		}

		ImGui::End();
	}
	else if (LAA_State == LAADialogState::Finished)
	{
		// Prompted the user & finished performing patches, report the results back to them
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(400, 175), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(400, 175), ImVec2(400, 175));
		if (LAA_ErrorNum == 0)
		{
			ImGui::Begin("Game 4GB patched successfully!");
			ImGui::TextWrapped("re4_tweaks has successfully patched your game EXE (a backup has also been made)\n\nPlease relaunch the game for the patch to take effect!");
		}
		else
		{
			ImGui::Begin("Game 4GB patch failed...");
			ImGui::TextWrapped("re4_tweaks failed to patch the game EXE (error %d)\n\nYou can manually patch it yourself by using the \"NTCore 4GB Patch\" tool - an internet search should help find it!", LAA_ErrorNum);
		}

		ImGui::Dummy(ImVec2(0.0f, 15.0f));

		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 113);

		if (ImGui::Button("OK", ImVec2(104, 35)))
		{
			LAA_State = LAADialogState::NotShowing;
		}

		ImGui::End();
	}
}