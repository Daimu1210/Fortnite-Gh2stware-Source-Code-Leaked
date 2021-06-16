#include "win_utils.h"
#include "xor.hpp"
#include <dwmapi.h>
#include "Main.h"

#define OFFSET_UWORLD 0x95ECB60

ImFont* m_pFont;

DWORD_PTR Uworld;
DWORD_PTR LocalPawn;
DWORD_PTR PlayerState;
DWORD_PTR Localplayer;
DWORD_PTR Rootcomp;
DWORD_PTR PlayerController;
DWORD_PTR Persistentlevel;
DWORD_PTR Ulevel;

Vector3 localactorpos;

uint64_t TargetPawn;
int localplayerID;

bool isaimbotting;

RECT GameRect = { NULL };
D3DPRESENT_PARAMETERS d3dpp;

DWORD ScreenCenterX;
DWORD ScreenCenterY;
DWORD ScreenCenterZ;

static void xCreateWindow();
static void xInitD3d();
static void xMainLoop();
static LRESULT CALLBACK WinProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HWND Window = NULL;
IDirect3D9Ex* p_Object = NULL;
static LPDIRECT3DDEVICE9 D3dDevice = NULL;
static LPDIRECT3DVERTEXBUFFER9 TriBuf = NULL;

FTransform GetBoneIndex(DWORD_PTR mesh, int index){
	DWORD_PTR bonearray = read<DWORD_PTR>(DriverHandle, processID, mesh + 0x4A8);
	if (bonearray == NULL)	{
		bonearray = read<DWORD_PTR>(DriverHandle, processID, mesh + 0x4A8 + 0x10);
	}
	return read<FTransform>(DriverHandle, processID, bonearray + (index * 0x30));
}

Vector3 GetBoneWithRotation(DWORD_PTR mesh, int id) {
	FTransform bone = GetBoneIndex(mesh, id);
	FTransform ComponentToWorld = read<FTransform>(DriverHandle, processID, mesh + 0x1C0);
	D3DMATRIX Matrix;
	Matrix = MatrixMultiplication(bone.ToMatrixWithScale(), ComponentToWorld.ToMatrixWithScale());
	return Vector3(Matrix._41, Matrix._42, Matrix._43);
}

D3DMATRIX Matrix(Vector3 rot, Vector3 origin = Vector3(0, 0, 0)) {
	float radPitch = (rot.x * float(M_PI) / 180.f);
	float radYaw = (rot.y * float(M_PI) / 180.f);
	float radRoll = (rot.z * float(M_PI) / 180.f);

	float SP = sinf(radPitch);
	float CP = cosf(radPitch);
	float SY = sinf(radYaw);
	float CY = cosf(radYaw);
	float SR = sinf(radRoll);
	float CR = cosf(radRoll);

	D3DMATRIX matrix;
	matrix.m[0][0] = CP * CY;
	matrix.m[0][1] = CP * SY;
	matrix.m[0][2] = SP;
	matrix.m[0][3] = 0.f;

	matrix.m[1][0] = SR * SP * CY - CR * SY;
	matrix.m[1][1] = SR * SP * SY + CR * CY;
	matrix.m[1][2] = -SR * CP;
	matrix.m[1][3] = 0.f;

	matrix.m[2][0] = -(CR * SP * CY + SR * SY);
	matrix.m[2][1] = CY * SR - CR * SP * SY;
	matrix.m[2][2] = CR * CP;
	matrix.m[2][3] = 0.f;

	matrix.m[3][0] = origin.x;
	matrix.m[3][1] = origin.y;
	matrix.m[3][2] = origin.z;
	matrix.m[3][3] = 1.f;

	return matrix;
}

extern Vector3 CameraEXT(0, 0, 0);

Vector3 ProjectWorldToScreen(Vector3 WorldLocation) {
	Vector3 Screenlocation = Vector3(0, 0, 0);
	Vector3 Camera;

	auto chain69 = read<uintptr_t>(DriverHandle, processID, Localplayer + 0xa8);
	uint64_t chain699 = read<uintptr_t>(DriverHandle, processID, chain69 + 8);

	Camera.x = read<float>(DriverHandle, processID, chain699 + 0x7F8);
	Camera.y = read<float>(DriverHandle, processID, Rootcomp + 0x12C);

	float test = asin(Camera.x);
	float degrees = test * (180.0 / M_PI);
	Camera.x = degrees;

	if (Camera.y < 0)
		Camera.y = 360 + Camera.y;

	D3DMATRIX tempMatrix = Matrix(Camera);
	Vector3 vAxisX, vAxisY, vAxisZ;

	vAxisX = Vector3(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
	vAxisY = Vector3(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
	vAxisZ = Vector3(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);

	uint64_t chain = read<uint64_t>(DriverHandle, processID, Localplayer + 0x70);
	uint64_t chain1 = read<uint64_t>(DriverHandle, processID, chain + 0x98);
	uint64_t chain2 = read<uint64_t>(DriverHandle, processID, chain1 + 0x130);

	Vector3 vDelta = WorldLocation - read<Vector3>(DriverHandle, processID, chain2 + 0x10);
	Vector3 vTransformed = Vector3(vDelta.Dot(vAxisY), vDelta.Dot(vAxisZ), vDelta.Dot(vAxisX));

	if (vTransformed.z < 1.f)
		vTransformed.z = 1.f;

	float zoom = read<float>(DriverHandle, processID, chain699 + 0x590);

	float FovAngle = 80.0f / (zoom / 1.19f);

	float ScreenCenterX = Width / 2;
	float ScreenCenterY = Height / 2;
	float ScreenCenterZ = Height / 2;

	Screenlocation.x = ScreenCenterX + vTransformed.x * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;
	Screenlocation.y = ScreenCenterY - vTransformed.y * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;
	Screenlocation.z = ScreenCenterZ - vTransformed.z * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;

	return Screenlocation;
}

DWORD Menuthread(LPVOID in) {
	while (1) {
		if (GetAsyncKeyState(VK_INSERT) & 1) {
			item.Show_Menu = !item.Show_Menu;
		}
		Sleep(2);
	}
}

Vector3 AimbotCorrection(float bulletVelocity, float bulletGravity, float targetDistance, Vector3 targetPosition, Vector3 targetVelocity) {
	Vector3 recalculated = targetPosition;
	float gravity = fabs(bulletGravity);
	float time = targetDistance / fabs(bulletVelocity);
	/* Bullet drop correction */
	float bulletDrop = (gravity / 250) * time * time;
	recalculated.z += bulletDrop * 120;
	/* Player movement correction */
	recalculated.x += time * (targetVelocity.x);
	recalculated.y += time * (targetVelocity.y);
	recalculated.z += time * (targetVelocity.z);
	return recalculated;
}

void aimbot(float x, float y, float z) {
	float ScreenCenterX = (Width / 2);
	float ScreenCenterY = (Height / 2);
	float ScreenCenterZ = (Depth / 2);
	int AimSpeed = item.Aim_Speed;
	float TargetX = 0;
	float TargetY = 0;
	float TargetZ = 0;

	if (x != 0) {
		if (x > ScreenCenterX) {
			TargetX = -(ScreenCenterX - x);
			TargetX /= AimSpeed;
			if (TargetX + ScreenCenterX > ScreenCenterX * 2) TargetX = 0;
		}

		if (x < ScreenCenterX) {
			TargetX = x - ScreenCenterX;
			TargetX /= AimSpeed;
			if (TargetX + ScreenCenterX < 0) TargetX = 0;
		}
	}

	if (y != 0) {
		if (y > ScreenCenterY) {
			TargetY = -(ScreenCenterY - y);
			TargetY /= AimSpeed;
			if (TargetY + ScreenCenterY > ScreenCenterY * 2) TargetY = 0;
		}

		if (y < ScreenCenterY) {
			TargetY = y - ScreenCenterY;
			TargetY /= AimSpeed;
			if (TargetY + ScreenCenterY < 0) TargetY = 0;
		}
	}

	if (z != 0) {
		if (z > ScreenCenterZ) {
			TargetZ = -(ScreenCenterZ - z);
			TargetZ /= AimSpeed;
			if (TargetZ + ScreenCenterZ > ScreenCenterZ * 2) TargetZ = 0;
		}

		if (z < ScreenCenterZ) {
			TargetZ = z - ScreenCenterZ;
			TargetZ /= AimSpeed;
			if (TargetZ + ScreenCenterZ < 0) TargetZ = 0;
		}
	}

	//WriteAngles(TargetX / 3.5f, TargetY / 3.5f, TargetZ / 3.5f);
	mouse_event(MOUSEEVENTF_MOVE, static_cast<DWORD>(TargetX), static_cast<DWORD>(TargetY), NULL, NULL);
	if (item.Auto_Fire) {
		mouse_event(MOUSEEVENTF_LEFTDOWN, DWORD(NULL), DWORD(NULL), DWORD(0x0002), ULONG_PTR(NULL));
		mouse_event(MOUSEEVENTF_LEFTUP, DWORD(NULL), DWORD(NULL), DWORD(0x0004), ULONG_PTR(NULL));
	}

	
	return;
}

double GetCrossDistance(double x1, double y1, double z1, double x2, double y2, double z2) {
	return sqrt(pow((x2 - x1), 2) + pow((y2 - y1), 2));
}

typedef struct _FNlEntity {
	uint64_t Actor;
	int ID;
	uint64_t mesh;
}FNlEntity;

std::vector<FNlEntity> entityList;

void cache()
{
	while (true) {
		std::vector<FNlEntity> tmpList;

		Uworld = read<DWORD_PTR>(DriverHandle, processID, base_address + OFFSET_UWORLD);
		DWORD_PTR Gameinstance = read<DWORD_PTR>(DriverHandle, processID, Uworld + 0x180);
		DWORD_PTR LocalPlayers = read<DWORD_PTR>(DriverHandle, processID, Gameinstance + 0x38);
		Localplayer = read<DWORD_PTR>(DriverHandle, processID, LocalPlayers);
		PlayerController = read<DWORD_PTR>(DriverHandle, processID, Localplayer + 0x30);
		LocalPawn = read<DWORD_PTR>(DriverHandle, processID, PlayerController + 0x2A0);

		PlayerState = read<DWORD_PTR>(DriverHandle, processID, LocalPawn + 0x240);
		Rootcomp = read<DWORD_PTR>(DriverHandle, processID, LocalPawn + 0x130);

		if (LocalPawn != 0) {
			localplayerID = read<int>(DriverHandle, processID, LocalPawn + 0x18);
		}

		Persistentlevel = read<DWORD_PTR>(DriverHandle, processID, Uworld + 0x30);
		DWORD ActorCount = read<DWORD>(DriverHandle, processID, Persistentlevel + 0xA0);
		DWORD_PTR AActors = read<DWORD_PTR>(DriverHandle, processID, Persistentlevel + 0x98);

		for (int i = 0; i < ActorCount; i++) {
			uint64_t CurrentActor = read<uint64_t>(DriverHandle, processID, AActors + i * 0x8);

			int curactorid = read<int>(DriverHandle, processID, CurrentActor + 0x18);

			if (curactorid == localplayerID || curactorid == localplayerID + 765) {
				FNlEntity fnlEntity{ };
				fnlEntity.Actor = CurrentActor;
				fnlEntity.mesh = read<uint64_t>(DriverHandle, processID, CurrentActor + 0x280);
				fnlEntity.ID = curactorid;
				tmpList.push_back(fnlEntity);
			}
		}
		entityList = tmpList;
		Sleep(1);
	}
}

void AimAt(DWORD_PTR entity) {
	uint64_t currentactormesh = read<uint64_t>(DriverHandle, processID, entity + 0x280);
	auto rootHead = GetBoneWithRotation(currentactormesh, item.hitbox);
	//Vector3 rootHeadOut = ProjectWorldToScreen(rootHead);

	if (item.Aim_Prediction) {
		float distance = localactorpos.Distance(rootHead) / 250;
		uint64_t CurrentActorRootComponent = read<uint64_t>(DriverHandle, processID, entity + 0x130);
		Vector3 vellocity = read<Vector3>(DriverHandle, processID, CurrentActorRootComponent + 0x140);
		Vector3 Predicted = AimbotCorrection(30000, -504, distance, rootHead, vellocity);
		Vector3 rootHeadOut = ProjectWorldToScreen(Predicted);
		if (rootHeadOut.x != 0 || rootHeadOut.y != 0 || rootHeadOut.z != 0) {
			if ((GetCrossDistance(rootHeadOut.x, rootHeadOut.y, rootHeadOut.z, Width / 2, Height / 2, Depth / 2) <= item.AimFOV * 1)) {
				aimbot(rootHeadOut.x, rootHeadOut.y, rootHeadOut.z);				
			}
		}
	}
	else {
		Vector3 rootHeadOut = ProjectWorldToScreen(rootHead);
		if (rootHeadOut.x != 0 || rootHeadOut.y != 0 || rootHeadOut.z != 0) {
			if ((GetCrossDistance(rootHeadOut.x, rootHeadOut.y, rootHeadOut.z, Width / 2, Height / 2, Depth / 2) <= item.AimFOV * 1)) {
				aimbot(rootHeadOut.x, rootHeadOut.y, rootHeadOut.z);
			}
		}
	}
}

void AimAt2(DWORD_PTR entity) {
	uint64_t currentactormesh = read<uint64_t>(DriverHandle, processID, entity + 0x280);
	auto rootHead = GetBoneWithRotation(currentactormesh, item.hitbox);

	if (item.Aim_Prediction) {
		float distance = localactorpos.Distance(rootHead) / 250;
		uint64_t CurrentActorRootComponent = read<uint64_t>(DriverHandle, processID, entity + 0x130);
		Vector3 vellocity = read<Vector3>(DriverHandle, processID, CurrentActorRootComponent + 0x140);
		Vector3 Predicted = AimbotCorrection(30000, -504, distance, rootHead, vellocity);
		Vector3 rootHeadOut = ProjectWorldToScreen(Predicted);
		if (rootHeadOut.x != 0 || rootHeadOut.y != 0 || rootHeadOut.z != 0) {
			if ((GetCrossDistance(rootHeadOut.x, rootHeadOut.y, rootHeadOut.z, Width / 2, Height / 2, Depth / 2) <= item.AimFOV * 1)) {
				if (item.Lock_Line) {
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height / 2), ImVec2(rootHeadOut.x, rootHeadOut.y), ImGui::GetColorU32({ item.LockLine[0], item.LockLine[1], item.LockLine[2], 1.0f }), item.Thickness);
				}
			}
		}
	}
	else {
		Vector3 rootHeadOut = ProjectWorldToScreen(rootHead);
		if (rootHeadOut.x != 0 || rootHeadOut.y != 0 || rootHeadOut.z != 0) {
			if ((GetCrossDistance(rootHeadOut.x, rootHeadOut.y, rootHeadOut.z, Width / 2, Height / 2, Depth / 2) <= item.AimFOV * 1)) {
				if (item.Lock_Line) {
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height / 2), ImVec2(rootHeadOut.x, rootHeadOut.y), ImGui::GetColorU32({ item.LockLine[0], item.LockLine[1], item.LockLine[2], 1.0f }), item.Thickness);
				}
			}
		}
	}
}

void DrawESP() {
	if (item.Draw_FOV_Circle) {
		ImGui::GetOverlayDrawList()->AddCircle(ImVec2(ScreenCenterX, ScreenCenterY), float(item.AimFOV), ImGui::GetColorU32({ item.DrawFOVCircle[0], item.DrawFOVCircle[1], item.DrawFOVCircle[2], 1.0f }), item.Shape, item.Thickness);
	}
	if (item.Cross_Hair) {
		ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2 - 30, Height / 2), ImVec2(Width / 2 - 10, Height / 2), ImGui::GetColorU32({ item.CrossHair[0], item.CrossHair[1], item.CrossHair[2], 1.0f }), item.Thickness);
		ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2 + 30, Height / 2), ImVec2(Width / 2 + 10, Height / 2), ImGui::GetColorU32({ item.CrossHair[0], item.CrossHair[1], item.CrossHair[2], 1.0f }), item.Thickness);
		ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height / 2 - 30), ImVec2(Width / 2, Height / 2 - 15), ImGui::GetColorU32({ item.CrossHair[0], item.CrossHair[1], item.CrossHair[2], 1.0f }), item.Thickness);
		ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height / 2 + 30), ImVec2(Width / 2, Height / 2 + 15), ImGui::GetColorU32({ item.CrossHair[0], item.CrossHair[1], item.CrossHair[2], 1.0f }), item.Thickness);
	}

	char dist[64];
	sprintf_s(dist, "%.1f Fps\n", ImGui::GetIO().Framerate);
	ImGui::GetOverlayDrawList()->AddText(ImVec2(15, 15), ImGui::GetColorU32({ color.Black[0], color.Black[1], color.Black[2], 4.0f }), dist);

	auto entityListCopy = entityList;
	float closestDistance = FLT_MAX;
	DWORD_PTR closestPawn = NULL;

	DWORD_PTR AActors = read<DWORD_PTR>(DriverHandle, processID, Ulevel + 0x98);

	if (AActors == (DWORD_PTR)nullptr)
		return;

	for (unsigned long i = 0; i < entityListCopy.size(); ++i) {
		FNlEntity entity = entityListCopy[i];

		uint64_t CurrentActor = read<uint64_t>(DriverHandle, processID, AActors + i * 0x8);

		uint64_t CurActorRootComponent = read<uint64_t>(DriverHandle, processID, entity.Actor + 0x130);
		if (CurActorRootComponent == (uint64_t)nullptr || CurActorRootComponent == -1 || CurActorRootComponent == NULL)
			continue;

		Vector3 actorpos = read<Vector3>(DriverHandle, processID, CurActorRootComponent + 0x11C);
		Vector3 actorposW2s = ProjectWorldToScreen(actorpos);

		DWORD64 otherPlayerState = read<uint64_t>(DriverHandle, processID, entity.Actor + 0x240);
		if (otherPlayerState == (uint64_t)nullptr || otherPlayerState == -1 || otherPlayerState == NULL)
			continue;

		localactorpos = read<Vector3>(DriverHandle, processID, Rootcomp + 0x11C);

		Vector3 bone66 = GetBoneWithRotation(entity.mesh, 66);
		Vector3 bone0 = GetBoneWithRotation(entity.mesh, 0);

		Vector3 top = ProjectWorldToScreen(bone66);
		Vector3 aimbotspot = ProjectWorldToScreen(bone66);
		Vector3 bottom = ProjectWorldToScreen(bone0);

		Vector3 Head = ProjectWorldToScreen(Vector3(bone66.x, bone66.y, bone66.z + 15));

		float distance = localactorpos.Distance(bone66) / 100.f;
		float BoxHeight = (float)(Head.y - bottom.y);
		float CornerHeight = abs(Head.y - bottom.y);
		float CornerWidth = BoxHeight * 0.80;

		int MyTeamId = read<int>(DriverHandle, processID, PlayerState + 0xED0);
		int ActorTeamId = read<int>(DriverHandle, processID, otherPlayerState + 0xED0);
		int curactorid = read<int>(DriverHandle, processID, CurrentActor + 0x18);

		if (MyTeamId != ActorTeamId) {
			if (distance < item.VisDist) {
				if (item.Esp_line) {
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height / 100), ImVec2(Head.x, Head.y), ImGui::GetColorU32({ item.LineESP[0], item.LineESP[1], item.LineESP[2], 1.0f }), item.Thickness);
				}
				if (item.Head_dot) {
					ImGui::GetOverlayDrawList()->AddCircleFilled(ImVec2(Head.x, Head.y), float(BoxHeight / 25), ImGui::GetColorU32({ item.Headdot[0], item.Headdot[1], item.Headdot[2], item.Transparency }), 50);
				}
				if (item.Triangle_ESP_Filled) {
					ImGui::GetOverlayDrawList()->AddTriangleFilled(ImVec2(Head.x, Head.y - 45), ImVec2(bottom.x - (BoxHeight / 2), bottom.y), ImVec2(bottom.x + (BoxHeight / 2), bottom.y), ImGui::GetColorU32({ item.TriangleESPFilled[0], item.TriangleESPFilled[1], item.TriangleESPFilled[2], item.Transparency }));
				}
				if (item.Triangle_ESP) {
					ImGui::GetOverlayDrawList()->AddTriangle(ImVec2(Head.x, Head.y - 50), ImVec2(bottom.x - (BoxHeight / 2), bottom.y), ImVec2(bottom.x + (BoxHeight / 2), bottom.y), ImGui::GetColorU32({ item.TriangleESP[0], item.TriangleESP[1], item.TriangleESP[2], 1.0f }), item.Thickness);
				}
				if (item.Esp_box_fill) {
					ImGui::GetOverlayDrawList()->AddRectFilled(ImVec2(Head.x - (CornerWidth / 2), Head.y), ImVec2(bottom.x + (CornerWidth / 2), bottom.y), ImGui::GetColorU32({ item.Espboxfill[0], item.Espboxfill[1], item.Espboxfill[2], item.Transparency }));
				}
				if (item.Esp_box) {
					ImGui::GetOverlayDrawList()->AddRect(ImVec2(Head.x - (CornerWidth / 2), Head.y), ImVec2(bottom.x + (CornerWidth / 2), bottom.y), ImGui::GetColorU32({ item.Espbox[0], item.Espbox[1], item.Espbox[2], 1.0f }), 0, item.Thickness);
				}
				if (item.Esp_Corner_Box) {
					DrawCornerBox(Head.x - (CornerWidth / 2), Head.y, CornerWidth, CornerHeight, ImGui::GetColorU32({ item.BoxCornerESP[0], item.BoxCornerESP[1], item.BoxCornerESP[2], 1.0f }), item.Thickness);
				}
				if (item.Esp_Circle_Fill) {
					ImGui::GetOverlayDrawList()->AddCircleFilled(ImVec2(Head.x, Head.y), float(BoxHeight), ImGui::GetColorU32({ item.EspCircleFill[0], item.EspCircleFill[1], item.EspCircleFill[2], item.Transparency }), item.Shape);
				}
				if (item.Esp_Circle) {
					ImGui::GetOverlayDrawList()->AddCircle(ImVec2(Head.x, Head.y), float(BoxHeight), ImGui::GetColorU32({ item.EspCircle[0], item.EspCircle[1], item.EspCircle[2], 1.0f }), item.Shape, item.Thickness);
				}
				if (item.Distance_Esp) {
					char dist[64];
					sprintf_s(dist, "%.fM", distance);
					ImGui::GetOverlayDrawList()->AddText(ImVec2(bottom.x - 20, bottom.y), ImGui::GetColorU32({ color.Black[0], color.Black[1], color.Black[2], 4.0f }), dist);
				}
				if (item.Aimbot) {
					auto dx = aimbotspot.x - (Width / 2);
					auto dy = aimbotspot.y - (Height / 2);
					auto dz = aimbotspot.z - (Depth / 2);
					auto dist = sqrtf(dx * dx + dy * dy + dz * dz) / 100.0f;
					if (dist < item.AimFOV && dist < closestDistance) {
						closestDistance = dist;
						closestPawn = entity.Actor;
					}
				}
			}			
		}
		else if (CurActorRootComponent != CurrentActor) {
			if (distance > 2) {
				if (item.Team_Esp_line) {
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height / 1), ImVec2(bottom.x, bottom.y), ImGui::GetColorU32({ item.TeamLineESP[0], item.TeamLineESP[1], item.TeamLineESP[2], 1.0f }), item.Thickness);
				}
				if (item.Team_Head_dot) {
					ImGui::GetOverlayDrawList()->AddCircleFilled(ImVec2(Head.x, Head.y), float(BoxHeight / 25), ImGui::GetColorU32({ item.TeamHeaddot[0], item.TeamHeaddot[1], item.TeamHeaddot[2], item.Transparency }), 50);
				}
				if (item.Team_Triangle_ESP_Filled) {
					ImGui::GetOverlayDrawList()->AddTriangleFilled(ImVec2(Head.x, Head.y - 45), ImVec2(bottom.x - (BoxHeight / 2), bottom.y), ImVec2(bottom.x + (BoxHeight / 2), bottom.y), ImGui::GetColorU32({ item.TeamTriangleESPFilled[0], item.TeamTriangleESPFilled[1], item.TeamTriangleESPFilled[2], item.Transparency }));
				}
				if (item.Team_Triangle_ESP) {
					ImGui::GetOverlayDrawList()->AddTriangle(ImVec2(Head.x, Head.y - 50), ImVec2(bottom.x - (BoxHeight / 2), bottom.y), ImVec2(bottom.x + (BoxHeight / 2), bottom.y), ImGui::GetColorU32({ item.TeamTriangleESP[0], item.TeamTriangleESP[1], item.TeamTriangleESP[2], 1.0f }), item.Thickness);
				}
				if (item.Team_Esp_box_fill) {
					ImGui::GetOverlayDrawList()->AddRectFilled(ImVec2(Head.x - (CornerWidth / 2), Head.y), ImVec2(bottom.x + (CornerWidth / 2), bottom.y), ImGui::GetColorU32({ item.TeamEspboxfill[0], item.TeamEspboxfill[1], item.TeamEspboxfill[2], item.Transparency }));
				}
				if (item.Team_Esp_box) {
					ImGui::GetOverlayDrawList()->AddRect(ImVec2(Head.x - (CornerWidth / 2), Head.y), ImVec2(bottom.x + (CornerWidth / 2), bottom.y), ImGui::GetColorU32({ item.TeamEspbox[0], item.TeamEspbox[1], item.TeamEspbox[2], 1.0f }), 0, item.Thickness);
				}
				if (item.Team_Esp_Corner_Box) {
					DrawCornerBox(Head.x - (CornerWidth / 2), Head.y, CornerWidth, CornerHeight, ImGui::GetColorU32({ item.TeamBoxCornerESP[0], item.TeamBoxCornerESP[1], item.TeamBoxCornerESP[2], 1.0f }), item.Thickness);
				}
				if (item.Team_Esp_Circle_Fill) {
					ImGui::GetOverlayDrawList()->AddCircleFilled(ImVec2(Head.x, Head.y), float(BoxHeight), ImGui::GetColorU32({ item.TeamEspCircleFill[0], item.TeamEspCircleFill[1], item.TeamEspCircleFill[2], item.Transparency }), item.Shape);
				}
				if (item.Team_Esp_Circle) {
					ImGui::GetOverlayDrawList()->AddCircle(ImVec2(Head.x, Head.y), float(BoxHeight), ImGui::GetColorU32({ item.TeamEspCircle[0], item.TeamEspCircle[1], item.TeamEspCircle[2], 1.0f }), item.Shape, item.Thickness);
				}
				if (item.Team_Distance_Esp) {
					char dist[64];
					sprintf_s(dist, "%.fM", distance);
					ImGui::GetOverlayDrawList()->AddText(ImVec2(bottom.x - 15, bottom.y), ImGui::GetColorU32({ color.Black[0], color.Black[1], color.Black[2], 4.0f }), dist);
				}
				if (item.Team_Aimbot) {					
					auto dx = aimbotspot.x - (Width / 2);
					auto dy = aimbotspot.y - (Height / 2);
					auto dz = aimbotspot.z - (Depth / 2);
					auto dist = sqrtf(dx * dx + dy * dy + dz * dz) / 100.0f;
					if (dist < item.AimFOV && dist < closestDistance) {
						closestDistance = dist;
						closestPawn = entity.Actor;
					}
				}
			}			
		}	
		else if (curactorid == 18391356) {
			ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height / 1), ImVec2(bottom.x, bottom.y), ImGui::GetColorU32({ item.TeamLineESP[0], item.TeamLineESP[1], item.TeamLineESP[2], 1.0f }), item.Thickness);
		}
	}

	if (item.Aimbot) {
		if (closestPawn != 0) {
			if (item.Aimbot && closestPawn && GetAsyncKeyState(item.aimkey) < 0) {
				AimAt(closestPawn);					
				if (item.Auto_Bone_Switch) {

					item.boneswitch += 1;
					if (item.boneswitch == 700) {
						item.boneswitch = 0;
					}

					if (item.boneswitch == 0) {
						item.hitboxpos = 0; 
					}
					else if (item.boneswitch == 50) {
						item.hitboxpos = 1; 
					}
					else if (item.boneswitch == 100) {
						item.hitboxpos = 2; 
					}
					else if (item.boneswitch == 150) {
						item.hitboxpos = 3; 
					}
					else if (item.boneswitch == 200) {
						item.hitboxpos = 4; 
					}
					else if (item.boneswitch == 250) {
						item.hitboxpos = 5;
					}
					else if (item.boneswitch == 300) {
						item.hitboxpos = 6; 
					}
					else if (item.boneswitch == 350) {
						item.hitboxpos = 7; 
					}
					else if (item.boneswitch == 400) {
						item.hitboxpos = 6;
					}
					else if (item.boneswitch == 450) {
						item.hitboxpos = 5;
					}
					else if (item.boneswitch == 500) {
						item.hitboxpos = 4;
					}
					else if (item.boneswitch == 550) {
						item.hitboxpos = 3;
					}
					else if (item.boneswitch == 600) {
						item.hitboxpos = 2;
					}
					else if (item.boneswitch == 650) {
						item.hitboxpos = 1;
					}
				}
			}
			else {
				isaimbotting = false;
				AimAt2(closestPawn);
			}
		}
	}
}

void GetKey() {
	if (item.hitboxpos == 0) {
		item.hitbox = 66; //head
	}
	else if (item.hitboxpos == 1) {
		item.hitbox = 65; //neck
	}
	else if (item.hitboxpos == 2) {
		item.hitbox = 64; //CHEST_TOP
	}
	else if (item.hitboxpos == 3) {
		item.hitbox = 36; //CHEST_TOP
	}
	else if (item.hitboxpos == 4) {
		item.hitbox = 7; //chest
	}
	else if (item.hitboxpos == 5) {
		item.hitbox = 6; //CHEST_LOW
	}
	else if (item.hitboxpos == 6) {
		item.hitbox = 5; //TORSO
	}
	else if (item.hitboxpos == 7) {
		item.hitbox = 2; //pelvis
	}

	if (item.aimkeypos == 0) {
		item.aimkey = VK_RBUTTON;//left mouse button
	}
	else if (item.aimkeypos == 1) {
		item.aimkey = VK_LBUTTON;//right mouse button
	}
	else if (item.aimkeypos == 2) {
		item.aimkey = VK_CAPITAL;//right mouse button
	}

	DrawESP();
}

void Active() {
	ImGuiStyle* Style = &ImGui::GetStyle();
	Style->Colors[ImGuiCol_Button] = ImColor(55, 55, 55);
	Style->Colors[ImGuiCol_ButtonActive] = ImColor(55, 55, 55);
	Style->Colors[ImGuiCol_ButtonHovered] = ImColor(0, 0, 0);
}
void Hovered() { 
	ImGuiStyle* Style = &ImGui::GetStyle(); 
	Style->Colors[ImGuiCol_Button] = ImColor(0, 0, 0); 
	Style->Colors[ImGuiCol_ButtonActive] = ImColor(0, 0, 0); 
	Style->Colors[ImGuiCol_ButtonHovered] = ImColor(55, 55, 55); 
}

void Active1() { 
	ImGuiStyle* Style = &ImGui::GetStyle(); 
	Style->Colors[ImGuiCol_Button] = ImColor(0, 55, 0); 
	Style->Colors[ImGuiCol_ButtonActive] = ImColor(0, 55, 0); 
	Style->Colors[ImGuiCol_ButtonHovered] = ImColor(55, 0, 0); 
}
void Hovered1() { 
	ImGuiStyle* Style = &ImGui::GetStyle(); 
	Style->Colors[ImGuiCol_Button] = ImColor(55, 0, 0); 
	Style->Colors[ImGuiCol_ButtonActive] = ImColor(55, 0, 0); 
	Style->Colors[ImGuiCol_ButtonHovered] = ImColor(0, 55, 0); 
}

void render() {
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (item.Show_Menu) {
		ImGuiStyle* Style = &ImGui::GetStyle();
		Style->WindowRounding = 0;
		Style->WindowBorderSize = 0;
		Style->ChildRounding = 0;
		Style->FrameBorderSize = 0;
		Style->Colors[ImGuiCol_WindowBg] = ImColor(0, 0, 0, 0);
		Style->Colors[ImGuiCol_ChildBg] = ImColor(19, 22, 27);
		Style->Colors[ImGuiCol_Button] = ImColor(25, 30, 255);
		Style->Colors[ImGuiCol_ButtonHovered] = ImColor(0, 30, 34);
		Style->Colors[ImGuiCol_ButtonActive] = ImColor(0, 0, 255);

		static int Active_SubTabRage = 1;
		static int Active_SubTabVisuals = 1;
		static int Active_SubTabMisc = 1;
		static int Active_SubTabSkins = 1;
		static int Active_SubTabConfigs = 1;

		std::string title = ("##DICK LINE");
		ImGuiWindowFlags TargetFlags;
		TargetFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;

		static short tab = 0;

		//MAIN WINDOW
		if (ImGui::Begin(title.c_str(), 0, TargetFlags)) {
			ImGui::SetWindowSize(ImVec2(1920, 1080), ImGuiCond_Once); {
				ImGuiStyle* style = &ImGui::GetStyle();
				style->WindowPadding = ImVec2(15, 15);
				style->WindowRounding = 5.0f;
				style->FramePadding = ImVec2(5, 5);
				style->FrameRounding = 4.0f;
				style->ItemSpacing = ImVec2(12, 8);
				style->ItemInnerSpacing = ImVec2(8, 6);
				style->IndentSpacing = 25.0f;
				style->ScrollbarSize = 15.0f;
				style->ScrollbarRounding = 9.0f;
				style->GrabMinSize = 5.0f;
				style->GrabRounding = 3.0f;

				ImGuiWindowFlags TargetFlags1;
				TargetFlags1 = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

				//-----------------------------------------------------------------------------------------------------------------
				// 4 botton menu
			    if (item.Active_Tab == 0) {
					Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
					ImGui::BeginChild("##Back1", ImVec2{ 430, 46 }, false), TargetFlags1; {

						// back ground
						Style->Colors[ImGuiCol_ChildBg] = ImColor(55, 55, 55);
						ImGui::SetCursorPos(ImVec2{ 4, 4 });
						ImGui::BeginChild("##back2", ImVec2{ 422, 38 }, false), TargetFlags1; {

							//botton 1
							ImGui::SetCursorPos(ImVec2{ 4, 4 });
							if (item.Active_Tab == 1) {
								Active();
							}
							else {
								Hovered();
							}
							if (ImGui::Button("AIMBOT", ImVec2{ 130, 30 })) {
								if (item.Active_Tab == 1) {
									item.Active_Tab = 0;
								}
								else {
									item.Active_Tab = 1;
								}
							}

							// line after botton
							Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
							ImGui::SetCursorPos(ImVec2{ 138, 0 });
							ImGui::BeginChild("##line1", ImVec2{ 4, 38 }, false), TargetFlags1; {}
							ImGui::EndChild();

							// botton 2
							ImGui::SetCursorPos(ImVec2{ 146, 4 });
							if (item.Active_Tab == 2) {
								Active();
							}
							else {
								Hovered();
							}
							if (ImGui::Button("ESP", ImVec2{ 130, 30 })) {
								if (item.Active_Tab == 2) {
									item.Active_Tab = 0;
								}
								else {
									item.Active_Tab = 2;
								}
							}

							// line after botton
							Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
							ImGui::SetCursorPos(ImVec2{ 280, 0 });
							ImGui::BeginChild("##line2", ImVec2{ 4, 38 }, false), TargetFlags1; {}
							ImGui::EndChild();

							// botton 4
							ImGui::SetCursorPos(ImVec2{ 288, 4 });
							if (item.Active_Tab == 3) {
								Active();
							}
							else {
								Hovered();
							}
							if (ImGui::Button("COLORS", ImVec2{ 130, 30 })) {
								if (item.Active_Tab == 3) {
									item.Active_Tab = 0;
								}
								else {
									item.Active_Tab = 3;
								}
							}
						}
						ImGui::EndChild();
					}
					ImGui::EndChild();
				}

				// aimbot menu
				if (item.Active_Tab == 1) {
					Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
					ImGui::BeginChild("##Back1", ImVec2{ 638, 450 }, false), TargetFlags1; { //--------------
						//left back ground
						Style->Colors[ImGuiCol_ChildBg] = ImColor(55, 55, 55);
						ImGui::SetCursorPos(ImVec2{ 4, 4 });
						ImGui::BeginChild("##back3", ImVec2{ 100, 442 }, false), TargetFlags1; { //--------------

							ImGui::SetCursorPos(ImVec2{ 30, 4 });
							ImGui::Text("GH2STWARE (PAID)");
							ImGui::SetCursorPos(ImVec2{ 35, 20 });
							ImGui::Text("GH2STWARE (PAID)");

							Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
							ImGui::SetCursorPos(ImVec2{ 0, 38 });
							ImGui::BeginChild("##back4", ImVec2{ 100, 4 }, false), TargetFlags1; { //--------------
							}
							ImGui::EndChild();

							ImGui::SetCursorPos(ImVec2{ 4, 46 });
							if (item.Switch_Page == 0) {
								Active1();
							}
							else {
								Hovered1();
							}
							if (ImGui::Button("ENEMY", ImVec2{ 92, 128 })) {
								if (item.Switch_Page == 0) {
									item.Switch_Page = 0;
								}
								else {
									item.Switch_Page = 0;
								}
							}

							ImGui::SetCursorPos(ImVec2{ 4, 178 });
							if (item.Switch_Page == 1) {
								Active1();
							}
							else {
								Hovered1();
							}
							if (ImGui::Button("TEAM", ImVec2{ 92, 128 })) {
								if (item.Switch_Page == 1) {
									item.Switch_Page = 0;
								}
								else {
									item.Switch_Page = 1;
								}
							}

							ImGui::SetCursorPos(ImVec2{ 4, 310 });
							if (item.Switch_Page == 2) {
								Active1();
							}
							else {
								Hovered1();
							}
							if (ImGui::Button("MISC", ImVec2{ 92, 128 })) {
								if (item.Switch_Page == 2) {
									item.Switch_Page = 0;
								}
								else {
									item.Switch_Page = 2;
								}
							}
						}
						ImGui::EndChild();
						// top back ground
						Style->Colors[ImGuiCol_ChildBg] = ImColor(55, 55, 55);
						ImGui::SetCursorPos(ImVec2{ 108, 4 });
						ImGui::BeginChild("##back2", ImVec2{ 422, 38 }, false), TargetFlags1; {

							//botton 1
							ImGui::SetCursorPos(ImVec2{ 4, 4 });
							if (item.Active_Tab == 1) {
								Active();
							}
							else {
								Hovered();
							}
							if (ImGui::Button("AIMBOT", ImVec2{ 130, 30 })) {
								if (item.Active_Tab == 1) {
									item.Active_Tab = 0;
								}
								else {
									item.Active_Tab = 1;
								}
							}

							// line after botton
							Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
							ImGui::SetCursorPos(ImVec2{ 138, 0 });
							ImGui::BeginChild("##line1", ImVec2{ 4, 38 }, false), TargetFlags1; {}
							ImGui::EndChild();

							// botton 2
							ImGui::SetCursorPos(ImVec2{ 146, 4 });
							if (item.Active_Tab == 2) {
								Active();
							}
							else {
								Hovered();
							}
							if (ImGui::Button("ESP", ImVec2{ 130, 30 })) {
								if (item.Active_Tab == 2) {
									item.Active_Tab = 0;
								}
								else {
									item.Active_Tab = 2;
								}
							}

							// line after botton
							Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
							ImGui::SetCursorPos(ImVec2{ 280, 0 });
							ImGui::BeginChild("##line2", ImVec2{ 4, 38 }, false), TargetFlags1; {}
							ImGui::EndChild();

							// botton 4
							ImGui::SetCursorPos(ImVec2{ 288, 4 });
							if (item.Active_Tab == 3) {
								Active();
							}
							else {
								Hovered();
							}
							if (ImGui::Button("COLORS", ImVec2{ 130, 30 })) {
								if (item.Active_Tab == 3) {
									item.Active_Tab = 0;
								}
								else {
									item.Active_Tab = 3;
								}
							}
						}
						ImGui::EndChild();
						//main back ground
						Style->Colors[ImGuiCol_ChildBg] = ImColor(27, 27, 27);
						ImGui::SetCursorPos(ImVec2{ 108, 46 });
						ImGui::BeginChild("##back7", ImVec2{ 422, 400 }, false), TargetFlags1; { //--------------

							// enemy
							if (item.Switch_Page == 0) {
								ImGui::SetCursorPos(ImVec2{ 50, 30 });
								if (item.Aimbot == true) {
									Active1();
								}
								else if (item.Aimbot == false) {
									Hovered1();
								}
								if (ImGui::Button("Aimbot", ImVec2{ 150, 40 })) {
									if (item.Aimbot == true) {
										item.Aimbot = false;
									}
									else {
										item.Aimbot = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 230, 30 });
								if (item.Aim_Prediction == true) {
									Active1();
								}
								else if (item.Aim_Prediction == false) {
									Hovered1();
								}
								if (ImGui::Button("Aim Prediction", ImVec2{ 150, 40 })) {
									if (item.Aim_Prediction == true) {
										item.Aim_Prediction = false;
									}
									else {
										item.Aim_Prediction = true;
									}
								}								
							}

							// team
							if (item.Switch_Page == 1) {
								ImGui::SetCursorPos(ImVec2{ 136, 30 });
								if (item.Team_Aimbot == true) {
									Active1();
								}
								else if (item.Team_Aimbot == false) {
									Hovered1();
								}
								if (ImGui::Button("Aimbot", ImVec2{ 150, 40 })) {
									if (item.Team_Aimbot == true) {
										item.Team_Aimbot = false;
									}
									else {
										item.Team_Aimbot = true;
									}
								}								
							}

							// misc
							if (item.Switch_Page == 2) {
								ImGui::SetCursorPos(ImVec2{ 50, 30 });
								if (item.Draw_FOV_Circle == true) {
									Active1();
								}
								else if (item.Draw_FOV_Circle == false) {
									Hovered1();
								}
								if (ImGui::Button("Aim FOV", ImVec2{ 150, 40 })) {
									if (item.Draw_FOV_Circle == true) {
										item.Draw_FOV_Circle = false;
									}
									else {
										item.Draw_FOV_Circle = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 230, 30 });
								if (item.Lock_Line == true) {
									Active1();
								}
								else if (item.Lock_Line == false) {
									Hovered1();
								}
								if (ImGui::Button("Lock Line", ImVec2{ 150, 40 })) {
									if (item.Lock_Line == true) {
										item.Lock_Line = false;
									}
									else {
										item.Lock_Line = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 50, 90 });
								if (item.Auto_Bone_Switch == true) {
									Active1();
								}
								else if (item.Auto_Bone_Switch == false) {
									Hovered1();
								}
								if (ImGui::Button("Auto Bone Switch", ImVec2{ 150, 40 })) {
									if (item.Auto_Bone_Switch == true) {
										item.Auto_Bone_Switch = false;
									}
									else {
										item.Auto_Bone_Switch = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 230, 90 });
								if (item.Cross_Hair == true) {
									Active1();
								}
								else if (item.Cross_Hair == false) {
									Hovered1();
								}
								if (ImGui::Button("Cross Hair", ImVec2{ 150, 40 })) {
									if (item.Cross_Hair == true) {
										item.Cross_Hair = false;
									}
									else {
										item.Cross_Hair = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 50, 150 });
								if (item.Auto_Fire == true) {
									Active1();
								}
								else if (item.Auto_Fire == false) {
									Hovered1();
								}
								if (ImGui::Button("Triggerbot", ImVec2{ 150, 40 })) {
									if (item.Auto_Fire == true) {
										item.Auto_Fire = false;
									}
									else {
										item.Auto_Fire = true;
									}
								}

								Style->Colors[ImGuiCol_ChildBg] = ImColor(27, 27, 27);
								ImGui::SetCursorPos(ImVec2{ 6, 200 });
								ImGui::BeginChild("##back74567", ImVec2{ 400, 35 }, false), TargetFlags1; { //--------------

									ImGui::SetCursorPos(ImVec2{ 10, 10 });
									if (ImGui::ArrowButton("##Aimbot FOV Left", ImGuiDir_Left)) {
										item.AimFOV -= 1;
										if (item.AimFOV < 1) {
											item.AimFOV = 1;
										}
									}

									ImGui::SetCursorPos(ImVec2{ 70, 10 });
									ImGui::SliderFloat("##Aimbot FOV", &item.AimFOV, 1, 1500);

									ImGui::SetCursorPos(ImVec2{ 100, 15 });
									ImGui::Text("Aimbot FOV");

									ImGui::SetCursorPos(ImVec2{ 370, 10 });
									if (ImGui::ArrowButton("##Aimbot FOV Right", ImGuiDir_Right)) {
										item.AimFOV += 1;
										if (item.AimFOV > 1500) {
											item.AimFOV = 1500;
										}
									}
								}
								ImGui::EndChild();

								Style->Colors[ImGuiCol_ChildBg] = ImColor(27, 27, 27);
								ImGui::SetCursorPos(ImVec2{ 6, 240 });
								ImGui::BeginChild("##back74hfr567", ImVec2{ 400, 35 }, false), TargetFlags1; { //--------------

									ImGui::SetCursorPos(ImVec2{ 10, 10 });
									if (ImGui::ArrowButton("##Aim_Speed Left", ImGuiDir_Left)) {
										item.Aim_Speed -= 0.1;
										if (item.Aim_Speed < 1) {
											item.Aim_Speed = 1;
										}
									}

									ImGui::SetCursorPos(ImVec2{ 70, 10 });
									ImGui::SliderFloat("##Aim_Speed", &item.Aim_Speed, 1, 50);

									ImGui::SetCursorPos(ImVec2{ 100, 15 });
									ImGui::Text("Aim Speed");

									ImGui::SetCursorPos(ImVec2{ 370, 10 });
									if (ImGui::ArrowButton("##Aim_Speed Right", ImGuiDir_Right)) {
										item.Aim_Speed += 0.1;
										if (item.Aim_Speed > 50) {
											item.Aim_Speed = 50;
										}
									}
								}
								ImGui::EndChild();

								Style->Colors[ImGuiCol_ChildBg] = ImColor(27, 27, 27);
								ImGui::SetCursorPos(ImVec2{ 0, 300 });
								ImGui::BeginChild("##back3", ImVec2{ 444, 100 }, false), TargetFlags1; { //--------------

									// aim key heding
									Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
									ImGui::SetCursorPos(ImVec2{ 60, 4 });
									ImGui::BeginChild("##back4", ImVec2{ 88, 28 }, false), TargetFlags1; {

										Style->Colors[ImGuiCol_ChildBg] = ImColor(27, 27, 27);
										ImGui::SetCursorPos(ImVec2{ 4, 4 });
										ImGui::BeginChild("##back5", ImVec2{ 80, 20 }, false), TargetFlags1; {

											ImGui::SetCursorPos(ImVec2{ 10, 2 });
											ImGui::Text("Aim Key's");
										}
										ImGui::EndChild();
									}
									ImGui::EndChild();

									// aim keys
									Style->Colors[ImGuiCol_ChildBg] = ImColor(27, 27, 27);
									ImGui::SetCursorPos(ImVec2{ 57, 35 });
									ImGui::BeginChild("##back6", ImVec2{ 90, 15 }, false), TargetFlags1; {
										ImGui::SetWindowFontScale(float{ 0.7f });
										if (item.aimkeypos == 0) {
											ImGui::SetCursorPos(ImVec2{ 22, 2 });
											ImGui::Text("Caps Lock");
										}
										if (item.aimkeypos == 2) {
											ImGui::SetCursorPos(ImVec2{ 7, 2 });
											ImGui::Text("Left Mouse Button");
										}
										if (item.aimkeypos == 1) {
											ImGui::SetCursorPos(ImVec2{ 4, 2 });
											ImGui::Text("Right Mouse Button");
										}
									}
									ImGui::EndChild();

									Style->Colors[ImGuiCol_ChildBg] = ImColor(27, 27, 27);
									ImGui::SetCursorPos(ImVec2{ 57, 67 });
									ImGui::BeginChild("##back9", ImVec2{ 90, 15 }, false), TargetFlags1; {
										ImGui::SetWindowFontScale(float{ 0.7f });
										if (item.aimkeypos == 1) {
											ImGui::SetCursorPos(ImVec2{ 22, 2 });
											ImGui::Text("Caps Lock");
										}
										if (item.aimkeypos == 0) {
											ImGui::SetCursorPos(ImVec2{ 7, 2 });
											ImGui::Text("Left Mouse Button");
										}
										if (item.aimkeypos == 2) {
											ImGui::SetCursorPos(ImVec2{ 4, 2 });
											ImGui::Text("Right Mouse Button");
										}
									}
									ImGui::EndChild();

									if (item.aimkeypos == 2) {
										ImGui::SetCursorPos(ImVec2{ 73, 50 });
										ImGui::Text("Caps Lock");
									}
									if (item.aimkeypos == 1) {
										ImGui::SetCursorPos(ImVec2{ 51, 50 });
										ImGui::Text("Left Mouse Button");
									}
									if (item.aimkeypos == 0) {
										ImGui::SetCursorPos(ImVec2{ 52, 50 });
										ImGui::Text("Right Mouse Button");
									}

									ImGui::SetCursorPos(ImVec2{ 12, 46 });
									if (ImGui::ArrowButton("##Aim Key Left", ImGuiDir_Down)) {
										item.aimkeypos -= 1;
										if (item.aimkeypos < 0) {
											item.aimkeypos = 2;
										}
									}

									ImGui::SetCursorPos(ImVec2{ 177, 46 });
									if (ImGui::ArrowButton("##Aim Key Right", ImGuiDir_Up)) {
										item.aimkeypos += 1;
										if (item.aimkeypos > 2) {
											item.aimkeypos = 0;
										}
									}
									//----------------------------------------------------------------------------------
									// 
									// bone key heding
									Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
									ImGui::SetCursorPos(ImVec2{ 272, 4 });
									ImGui::BeginChild("##back7", ImVec2{ 88, 28 }, false), TargetFlags1; {

										Style->Colors[ImGuiCol_ChildBg] = ImColor(27, 27, 27);
										ImGui::SetCursorPos(ImVec2{ 4, 4 });
										ImGui::BeginChild("##back8", ImVec2{ 80, 20 }, false), TargetFlags1; {

											ImGui::SetCursorPos(ImVec2{ 5, 2 });
											ImGui::Text("Target Bone");
										}
										ImGui::EndChild();
									}
									ImGui::EndChild();

									// aim keys
									Style->Colors[ImGuiCol_ChildBg] = ImColor(27, 27, 27);
									ImGui::SetCursorPos(ImVec2{ 284, 35 });
									ImGui::BeginChild("##back10", ImVec2{ 90, 15 }, false), TargetFlags1; {
										ImGui::SetWindowFontScale(float{ 0.7f });

										if (item.hitboxpos == 0) {
											ImGui::SetCursorPos(ImVec2{ 17, 2 });
											ImGui::Text("Pelvis");
										}
										if (item.hitboxpos == 7) {
											ImGui::SetCursorPos(ImVec2{ 17, 2 });
											ImGui::Text("Chest");
										}
										if (item.hitboxpos == 4) {
											ImGui::SetCursorPos(ImVec2{ 17, 2 });
											ImGui::Text("Neck");
										}
										if (item.hitboxpos == 1) {
											ImGui::SetCursorPos(ImVec2{ 17, 2 });
											ImGui::Text("Head");
										}
									}
									ImGui::EndChild();

									Style->Colors[ImGuiCol_ChildBg] = ImColor(27, 27, 27);
									ImGui::SetCursorPos(ImVec2{ 284, 67 });
									ImGui::BeginChild("##back11", ImVec2{ 90, 15 }, false), TargetFlags1; {
										ImGui::SetWindowFontScale(float{ 0.7f });

										if (item.hitboxpos == 4) {
											ImGui::SetCursorPos(ImVec2{ 17, 2 });
											ImGui::Text("Pelvis");
										}
										if (item.hitboxpos == 1) {
											ImGui::SetCursorPos(ImVec2{ 17, 2 });
											ImGui::Text("Chest");
										}
										if (item.hitboxpos == 0) {
											ImGui::SetCursorPos(ImVec2{ 17, 2 });
											ImGui::Text("Neck");
										}
										if (item.hitboxpos == 7) {
											ImGui::SetCursorPos(ImVec2{ 17, 2 });
											ImGui::Text("Head");
										}
									}
									ImGui::EndChild();

									if (item.hitboxpos == 7) {
										ImGui::SetCursorPos(ImVec2{ 299, 50 });
										ImGui::Text("Pelvis");
									}
									if (item.hitboxpos == 4) {
										ImGui::SetCursorPos(ImVec2{ 299, 50 });
										ImGui::Text("Chest");
									}
									if (item.hitboxpos == 1) {
										ImGui::SetCursorPos(ImVec2{ 299, 50 });
										ImGui::Text("Neck");
									}
									if (item.hitboxpos == 0) {
										ImGui::SetCursorPos(ImVec2{ 299, 50 });
										ImGui::Text("Head");
									}

									ImGui::SetCursorPos(ImVec2{ 224, 46 });
									if (ImGui::ArrowButton("##bone Key Left", ImGuiDir_Down)) {
										item.hitboxpos -= 1;
										if (item.hitboxpos < 0) {
											item.hitboxpos = 7;
										}
									}

									ImGui::SetCursorPos(ImVec2{ 384, 46 });
									if (ImGui::ArrowButton("##bone Key Right", ImGuiDir_Up)) {
										item.hitboxpos += 1;
										if (item.hitboxpos > 7) {
											item.hitboxpos = 0;
										}
									}
								}
								ImGui::EndChild();
							}
						}
						ImGui::EndChild();
						//right back ground
						Style->Colors[ImGuiCol_ChildBg] = ImColor(55, 55, 55);
						ImGui::SetCursorPos(ImVec2{ 534, 4 });
						ImGui::BeginChild("##back5", ImVec2{ 100, 442 }, false), TargetFlags1; { //--------------

							Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
							ImGui::SetCursorPos(ImVec2{ 0, 38 });
							ImGui::BeginChild("##back6", ImVec2{ 100, 4 }, false), TargetFlags1; { //--------------
							}
							ImGui::EndChild();
						}
						ImGui::EndChild();
					}
					ImGui::EndChild();
				}

				// esp menu
				if (item.Active_Tab == 2) {
					Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
					ImGui::BeginChild("##Back1", ImVec2{ 638, 450 }, false), TargetFlags1; { //--------------
						//left back ground
						Style->Colors[ImGuiCol_ChildBg] = ImColor(55, 55, 55);
						ImGui::SetCursorPos(ImVec2{ 4, 4 });
						ImGui::BeginChild("##back3", ImVec2{ 100, 442 }, false), TargetFlags1; { //--------------

							ImGui::SetCursorPos(ImVec2{ 30, 4 });
							ImGui::Text("GEORGE");
							ImGui::SetCursorPos(ImVec2{ 35, 20 });
							ImGui::Text("FLOYD FN");

							Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
							ImGui::SetCursorPos(ImVec2{ 0, 38 });
							ImGui::BeginChild("##back4", ImVec2{ 100, 4 }, false), TargetFlags1; { //--------------
							}
							ImGui::EndChild();

							ImGui::SetCursorPos(ImVec2{ 4, 46 });
							if (item.Switch_Page == 0) {
								Active1();
							}
							else {
								Hovered1();
							}
							if (ImGui::Button("ENEMY", ImVec2{ 92, 128 })) {
								if (item.Switch_Page == 0) {
									item.Switch_Page = 0;
								}
								else {
									item.Switch_Page = 0;
								}
							}

							ImGui::SetCursorPos(ImVec2{ 4, 178 });
							if (item.Switch_Page == 1) {
								Active1();
							}
							else {
								Hovered1();
							}
							if (ImGui::Button("TEAM", ImVec2{ 92, 128 })) {
								if (item.Switch_Page == 1) {
									item.Switch_Page = 0;
								}
								else {
									item.Switch_Page = 1;
								}
							}

							ImGui::SetCursorPos(ImVec2{ 4, 310 });
							if (item.Switch_Page == 2) {
								Active1();
							}
							else {
								Hovered1();
							}
							if (ImGui::Button("MISC", ImVec2{ 92, 128 })) {
								if (item.Switch_Page == 2) {
									item.Switch_Page = 0;
								}
								else {
									item.Switch_Page = 2;
								}
							}
						}
						ImGui::EndChild();
						// top back ground
						Style->Colors[ImGuiCol_ChildBg] = ImColor(55, 55, 55);
						ImGui::SetCursorPos(ImVec2{ 108, 4 });
						ImGui::BeginChild("##back2", ImVec2{ 422, 38 }, false), TargetFlags1; {

							//botton 1
							ImGui::SetCursorPos(ImVec2{ 4, 4 });
							if (item.Active_Tab == 1) {
								Active();
							}
							else {
								Hovered();
							}
							if (ImGui::Button("AIMBOT", ImVec2{ 130, 30 })) {
								if (item.Active_Tab == 1) {
									item.Active_Tab = 0;
								}
								else {
									item.Active_Tab = 1;
								}
							}

							// line after botton
							Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
							ImGui::SetCursorPos(ImVec2{ 138, 0 });
							ImGui::BeginChild("##line1", ImVec2{ 4, 38 }, false), TargetFlags1; {}
							ImGui::EndChild();

							// botton 2
							ImGui::SetCursorPos(ImVec2{ 146, 4 });
							if (item.Active_Tab == 2) {
								Active();
							}
							else {
								Hovered();
							}
							if (ImGui::Button("ESP", ImVec2{ 130, 30 })) {
								if (item.Active_Tab == 2) {
									item.Active_Tab = 0;
								}
								else {
									item.Active_Tab = 2;
								}
							}

							// line after botton
							Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
							ImGui::SetCursorPos(ImVec2{ 280, 0 });
							ImGui::BeginChild("##line2", ImVec2{ 4, 38 }, false), TargetFlags1; {}
							ImGui::EndChild();

							// botton 4
							ImGui::SetCursorPos(ImVec2{ 288, 4 });
							if (item.Active_Tab == 3) {
								Active();
							}
							else {
								Hovered();
							}
							if (ImGui::Button("COLORS", ImVec2{ 130, 30 })) {
								if (item.Active_Tab == 3) {
									item.Active_Tab = 0;
								}
								else {
									item.Active_Tab = 3;
								}
							}
						}
						ImGui::EndChild();
						//main back ground
						Style->Colors[ImGuiCol_ChildBg] = ImColor(27, 27, 27);
						ImGui::SetCursorPos(ImVec2{ 108, 46 });
						ImGui::BeginChild("##back7", ImVec2{ 422, 400 }, false), TargetFlags1; { //--------------

							// enemy
							if (item.Switch_Page == 0) {
								ImGui::SetCursorPos(ImVec2{ 10, 10 });
								if (item.Esp_box == true) {
									Active1();
								}
								else if (item.Esp_box == false) {
									Hovered1();
								}
								if (ImGui::Button("Box", ImVec2{ 126, 30 })) {
									if (item.Esp_box == true) {
										item.Esp_box = false;
									}
									else {
										item.Esp_box = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 10, 50 });
								if (item.Esp_Corner_Box == true) {
									Active1();
								}
								else if (item.Esp_Corner_Box == false) {
									Hovered1();
								}
								if (ImGui::Button("Corner Box", ImVec2{ 126, 30 })) {
									if (item.Esp_Corner_Box == true) {
										item.Esp_Corner_Box = false;
									}
									else {
										item.Esp_Corner_Box = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 10, 90 });
								if (item.Esp_box_fill == true) {
									Active1();
								}
								else if (item.Esp_box_fill == false) {
									Hovered1();
								}
								if (ImGui::Button("Box Fill", ImVec2{ 126, 30 })) {
									if (item.Esp_box_fill == true) {
										item.Esp_box_fill = false;
									}
									else {
										item.Esp_box_fill = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 146, 10 });
								if (item.Esp_Circle == true) {
									Active1();
								}
								else if (item.Esp_Circle == false) {
									Hovered1();
								}
								if (ImGui::Button("Circle", ImVec2{ 130, 30 })) {
									if (item.Esp_Circle == true) {
										item.Esp_Circle = false;
									}
									else {
										item.Esp_Circle = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 146, 50 });
								if (item.Esp_Circle_Fill == true) {
									Active1();
								}
								else if (item.Esp_Circle_Fill == false) {
									Hovered1();
								}
								if (ImGui::Button("Circle Fill", ImVec2{ 130, 30 })) {
									if (item.Esp_Circle_Fill == true) {
										item.Esp_Circle_Fill = false;
									}
									else {
										item.Esp_Circle_Fill = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 146, 90 });
								if (item.Triangle_ESP == true) {
									Active1();
								}
								else if (item.Triangle_ESP == false) {
									Hovered1();
								}
								if (ImGui::Button("Triangle", ImVec2{ 130, 30 })) {
									if (item.Triangle_ESP == true) {
										item.Triangle_ESP = false;
									}
									else {
										item.Triangle_ESP = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 146, 130 });
								if (item.Triangle_ESP_Filled == true) {
									Active1();
								}
								else if (item.Triangle_ESP_Filled == false) {
									Hovered1();
								}
								if (ImGui::Button("Triangle Filled", ImVec2{ 130, 30 })) {
									if (item.Triangle_ESP_Filled == true) {
										item.Triangle_ESP_Filled = false;
									}
									else {
										item.Triangle_ESP_Filled = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 286, 10 });
								if (item.Distance_Esp == true) {
									Active1();
								}
								else if (item.Distance_Esp == false) {
									Hovered1();
								}
								if (ImGui::Button("Distance", ImVec2{ 126, 30 })) {
									if (item.Distance_Esp == true) {
										item.Distance_Esp = false;
									}
									else {
										item.Distance_Esp = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 286, 50 });
								if (item.Esp_line == true) {
									Active1();
								}
								else if (item.Esp_line == false) {
									Hovered1();
								}
								if (ImGui::Button("Snap Line", ImVec2{ 126, 30 })) {
									if (item.Esp_line == true) {
										item.Esp_line = false;
									}
									else {
										item.Esp_line = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 286, 90 });
								if (item.Head_dot == true) {
									Active1();
								}
								else if (item.Head_dot == false) {
									Hovered1();
								}
								if (ImGui::Button("Head Dot", ImVec2{ 126, 30 })) {
									if (item.Head_dot == true) {
										item.Head_dot = false;
									}
									else {
										item.Head_dot = true;
									}
								}
							}

							// team
							if (item.Switch_Page == 1) {
								ImGui::SetCursorPos(ImVec2{ 10, 10 });
								if (item.Team_Esp_box == true) {
									Active1();
								}
								else if (item.Team_Esp_box == false) {
									Hovered1();
								}
								if (ImGui::Button("Box", ImVec2{ 126, 30 })) {
									if (item.Team_Esp_box == true) {
										item.Team_Esp_box = false;
									}
									else {
										item.Team_Esp_box = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 10, 50 });
								if (item.Team_Esp_Corner_Box == true) {
									Active1();
								}
								else if (item.Team_Esp_Corner_Box == false) {
									Hovered1();
								}
								if (ImGui::Button("Corner Box", ImVec2{ 126, 30 })) {
									if (item.Team_Esp_Corner_Box == true) {
										item.Team_Esp_Corner_Box = false;
									}
									else {
										item.Team_Esp_Corner_Box = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 10, 90 });
								if (item.Team_Esp_box_fill == true) {
									Active1();
								}
								else if (item.Team_Esp_box_fill == false) {
									Hovered1();
								}
								if (ImGui::Button("Box Fill", ImVec2{ 126, 30 })) {
									if (item.Team_Esp_box_fill == true) {
										item.Team_Esp_box_fill = false;
									}
									else {
										item.Team_Esp_box_fill = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 146, 10 });
								if (item.Team_Esp_Circle == true) {
									Active1();
								}
								else if (item.Team_Esp_Circle == false) {
									Hovered1();
								}
								if (ImGui::Button("Circle", ImVec2{ 130, 30 })) {
									if (item.Team_Esp_Circle == true) {
										item.Team_Esp_Circle = false;
									}
									else {
										item.Team_Esp_Circle = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 146, 50 });
								if (item.Team_Esp_Circle_Fill == true) {
									Active1();
								}
								else if (item.Team_Esp_Circle_Fill == false) {
									Hovered1();
								}
								if (ImGui::Button("Circle Fill", ImVec2{ 130, 30 })) {
									if (item.Team_Esp_Circle_Fill == true) {
										item.Team_Esp_Circle_Fill = false;
									}
									else {
										item.Team_Esp_Circle_Fill = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 146, 90 });
								if (item.Team_Triangle_ESP == true) {
									Active1();
								}
								else if (item.Team_Triangle_ESP == false) {
									Hovered1();
								}
								if (ImGui::Button("Triangle", ImVec2{ 130, 30 })) {
									if (item.Team_Triangle_ESP == true) {
										item.Team_Triangle_ESP = false;
									}
									else {
										item.Team_Triangle_ESP = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 146, 130 });
								if (item.Team_Triangle_ESP_Filled == true) {
									Active1();
								}
								else if (item.Team_Triangle_ESP_Filled == false) {
									Hovered1();
								}
								if (ImGui::Button("Triangle Filled", ImVec2{ 130, 30 })) {
									if (item.Team_Triangle_ESP_Filled == true) {
										item.Team_Triangle_ESP_Filled = false;
									}
									else {
										item.Team_Triangle_ESP_Filled = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 286, 10 });
								if (item.Team_Distance_Esp == true) {
									Active1();
								}
								else if (item.Team_Distance_Esp == false) {
									Hovered1();
								}
								if (ImGui::Button("Distance", ImVec2{ 126, 30 })) {
									if (item.Team_Distance_Esp == true) {
										item.Team_Distance_Esp = false;
									}
									else {
										item.Team_Distance_Esp = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 286, 50 });
								if (item.Team_Esp_line == true) {
									Active1();
								}
								else if (item.Team_Esp_line == false) {
									Hovered1();
								}
								if (ImGui::Button("Snap Line", ImVec2{ 126, 30 })) {
									if (item.Team_Esp_line == true) {
										item.Team_Esp_line = false;
									}
									else {
										item.Team_Esp_line = true;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 286, 90 });
								if (item.Team_Head_dot == true) {
									Active1();
								}
								else if (item.Team_Head_dot == false) {
									Hovered1();
								}
								if (ImGui::Button("Head Dot", ImVec2{ 126, 30 })) {
									if (item.Team_Head_dot == true) {
										item.Team_Head_dot = false;
									}
									else {
										item.Team_Head_dot = true;
									}
								}
							}

							// misc
							if (item.Switch_Page == 2) {





							}
						}
						ImGui::EndChild();
						//right back ground
						Style->Colors[ImGuiCol_ChildBg] = ImColor(55, 55, 55);
						ImGui::SetCursorPos(ImVec2{ 534, 4 });
						ImGui::BeginChild("##back5", ImVec2{ 100, 442 }, false), TargetFlags1; { //--------------

							Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
							ImGui::SetCursorPos(ImVec2{ 0, 38 });
							ImGui::BeginChild("##back6", ImVec2{ 100, 4 }, false), TargetFlags1; { //--------------
							}
							ImGui::EndChild();
						}
						ImGui::EndChild();
					}
					ImGui::EndChild();
				}

				// color menu
				if (item.Active_Tab == 3) {
					Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
					ImGui::BeginChild("##Back1", ImVec2{ 638, 450 }, false), TargetFlags1; { //--------------
						//left back ground
						Style->Colors[ImGuiCol_ChildBg] = ImColor(55, 55, 55);
						ImGui::SetCursorPos(ImVec2{ 4, 4 });
						ImGui::BeginChild("##back3", ImVec2{ 100, 442 }, false), TargetFlags1; { //--------------

							ImGui::SetCursorPos(ImVec2{ 30, 4 });
							ImGui::Text("GEORGE");
							ImGui::SetCursorPos(ImVec2{ 35, 20 });
							ImGui::Text("FLOYD FN");

							Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
							ImGui::SetCursorPos(ImVec2{ 0, 38 });
							ImGui::BeginChild("##back4", ImVec2{ 100, 4 }, false), TargetFlags1; { //--------------
							}
							ImGui::EndChild();

							ImGui::SetCursorPos(ImVec2{ 4, 46 });
							if (item.Switch_Page == 0) {
								Active1();
							}
							else {
								Hovered1();
							}
							if (ImGui::Button("ENEMY", ImVec2{ 92, 128 })) {
								if (item.Switch_Page == 0) {
									item.Switch_Page = 0;
								}
								else {
									item.Switch_Page = 0;
								}
							}

							ImGui::SetCursorPos(ImVec2{ 4, 178 });
							if (item.Switch_Page == 1) {
								Active1();
							}
							else {
								Hovered1();
							}
							if (ImGui::Button("TEAM", ImVec2{ 92, 128 })) {
								if (item.Switch_Page == 1) {
									item.Switch_Page = 0;
								}
								else {
									item.Switch_Page = 1;
								}
							}

							ImGui::SetCursorPos(ImVec2{ 4, 310 });
							if (item.Switch_Page == 2) {
								Active1();
							}
							else {
								Hovered1();
							}
							if (ImGui::Button("MISC", ImVec2{ 92, 128 })) {
								if (item.Switch_Page == 2) {
									item.Switch_Page = 0;
								}
								else {
									item.Switch_Page = 2;
								}
							}
						}
						ImGui::EndChild();
						// top back ground
						Style->Colors[ImGuiCol_ChildBg] = ImColor(55, 55, 55);
						ImGui::SetCursorPos(ImVec2{ 108, 4 });
						ImGui::BeginChild("##back2", ImVec2{ 422, 38 }, false), TargetFlags1; {

							//botton 1
							ImGui::SetCursorPos(ImVec2{ 4, 4 });
							if (item.Active_Tab == 1) {
								Active();
							}
							else {
								Hovered();
							}
							if (ImGui::Button("AIMBOT", ImVec2{ 130, 30 })) {
								if (item.Active_Tab == 1) {
									item.Active_Tab = 0;
								}
								else {
									item.Active_Tab = 1;
								}
							}

							// line after botton
							Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
							ImGui::SetCursorPos(ImVec2{ 138, 0 });
							ImGui::BeginChild("##line1", ImVec2{ 4, 38 }, false), TargetFlags1; {}
							ImGui::EndChild();

							// botton 2
							ImGui::SetCursorPos(ImVec2{ 146, 4 });
							if (item.Active_Tab == 2) {
								Active();
							}
							else {
								Hovered();
							}
							if (ImGui::Button("ESP", ImVec2{ 130, 30 })) {
								if (item.Active_Tab == 2) {
									item.Active_Tab = 0;
								}
								else {
									item.Active_Tab = 2;
								}
							}

							// line after botton
							Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
							ImGui::SetCursorPos(ImVec2{ 280, 0 });
							ImGui::BeginChild("##line2", ImVec2{ 4, 38 }, false), TargetFlags1; {}
							ImGui::EndChild();

							// botton 4
							ImGui::SetCursorPos(ImVec2{ 288, 4 });
							if (item.Active_Tab == 3) {
								Active();
							}
							else {
								Hovered();
							}
							if (ImGui::Button("COLORS", ImVec2{ 130, 30 })) {
								if (item.Active_Tab == 3) {
									item.Active_Tab = 0;
								}
								else {
									item.Active_Tab = 3;
								}
							}
						}
						ImGui::EndChild();
						//main back ground
						Style->Colors[ImGuiCol_ChildBg] = ImColor(27, 27, 27);
						ImGui::SetCursorPos(ImVec2{ 108, 46 });
						ImGui::BeginChild("##back7", ImVec2{ 422, 400 }, false), TargetFlags1; { //--------------

							// enemy
							if (item.Switch_Page == 0) {
								if (item.Color_Page == 1) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.DrawFOVCircle, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 2) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.Espbox, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 3) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.BoxCornerESP, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 4) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.Espboxfill, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 5) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.EspCircle, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 6) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.EspCircleFill, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 7) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.TriangleESP, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 8) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.TriangleESPFilled, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 9) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.Headdot, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 10) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.LineESP, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 11) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.CrossHair, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 12) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.LockLine, ImGuiColorEditFlags_NoInputs);
								}
							}

							// team
							if (item.Switch_Page == 1) {
								if (item.Color_Page == 1) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.DrawFOVCircle, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 2) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.TeamEspbox, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 3) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.TeamBoxCornerESP, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 4) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.TeamEspboxfill, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 5) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.TeamEspCircle, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 6) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.TeamEspCircleFill, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 7) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.TeamTriangleESP, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 8) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.TeamTriangleESPFilled, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 9) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.TeamHeaddot, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 10) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.TeamLineESP, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 11) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.CrossHair, ImGuiColorEditFlags_NoInputs);
								}

								if (item.Color_Page == 12) {
									ImGui::SetCursorPos(ImVec2{ 30, 20 });
									ImGui::ColorPicker3(("##DrawFOVCircle"), item.LockLine, ImGuiColorEditFlags_NoInputs);
								}
							}

							// misc
							if (item.Switch_Page == 2) {
								Style->Colors[ImGuiCol_ChildBg] = ImColor(27, 27, 27);
								ImGui::SetCursorPos(ImVec2{ 10, 20 });
								ImGui::BeginChild("##back74567", ImVec2{ 400, 50 }, false), TargetFlags1; { //--------------

									ImGui::SetCursorPos(ImVec2{ 10, 10 });
									if (ImGui::ArrowButton("##Thickness1", ImGuiDir_Left)) {
										item.Thickness -= 1;
										if (item.Thickness < 1) {
											item.Thickness = 1;
										}
									}

									ImGui::SetCursorPos(ImVec2{ 70, 10 });
									ImGui::SliderFloat("##Thickness2", &item.Thickness, 1, 20);

									ImGui::SetCursorPos(ImVec2{ 100, 15 });
									ImGui::Text("Thickness");

									ImGui::SetCursorPos(ImVec2{ 370, 10 });
									if (ImGui::ArrowButton("##Thickness3", ImGuiDir_Right)) {
										item.Thickness += 1;
										if (item.Thickness > 20) {
											item.Thickness = 20;
										}
									}
								}
								ImGui::EndChild();

								Style->Colors[ImGuiCol_ChildBg] = ImColor(27, 27, 27);
								ImGui::SetCursorPos(ImVec2{ 10, 90 });
								ImGui::BeginChild("##Shape764tdr7", ImVec2{ 400, 50 }, false), TargetFlags1; { //--------------

									ImGui::SetCursorPos(ImVec2{ 10, 10 });
									if (ImGui::ArrowButton("##Shyutfrape", ImGuiDir_Left)) {
										item.Shape -= 1;
										if (item.Shape < 7) {
											item.Shape = 7;
										}
									}

									ImGui::SetCursorPos(ImVec2{ 70, 10 });
									ImGui::SliderFloat("##Shgyhtfdrape78678", &item.Shape, 7, 50);

									ImGui::SetCursorPos(ImVec2{ 100, 15 });
									ImGui::Text("Shape");

									ImGui::SetCursorPos(ImVec2{ 370, 10 });
									if (ImGui::ArrowButton("##Shapyutuie2313", ImGuiDir_Right)) {
										item.Shape += 1;
										if (item.Shape > 60) {
											item.Shape = 60;
										}
									}
								}
								ImGui::EndChild();

								Style->Colors[ImGuiCol_ChildBg] = ImColor(27, 27, 27);
								ImGui::SetCursorPos(ImVec2{ 10, 160 });
								ImGui::BeginChild("##Shape7647", ImVec2{ 400, 50 }, false), TargetFlags1; { //--------------

									ImGui::SetCursorPos(ImVec2{ 10, 10 });
									if (ImGui::ArrowButton("##Shape", ImGuiDir_Left)) {
										item.Transparency -= 0.1;
										if (item.Transparency < 0) {
											item.Transparency = 0;
										}
									}

									ImGui::SetCursorPos(ImVec2{ 70, 10 });
									ImGui::SliderFloat("##Shape78678", &item.Transparency, 0, 50);

									ImGui::SetCursorPos(ImVec2{ 100, 15 });
									ImGui::Text("Transparency");

									ImGui::SetCursorPos(ImVec2{ 370, 10 });
									if (ImGui::ArrowButton("##Shape2313", ImGuiDir_Right)) {
										item.Transparency += 0.1;
										if (item.Transparency > 50) {
											item.Transparency = 50;
										}
									}
								}
								ImGui::EndChild();
							}
						}
						ImGui::EndChild();

						//right back ground
						Style->Colors[ImGuiCol_ChildBg] = ImColor(55, 55, 55);
						ImGui::SetCursorPos(ImVec2{ 534, 4 });
						ImGui::BeginChild("##back5", ImVec2{ 100, 442 }, false), TargetFlags1; { //--------------

							Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0);
							ImGui::SetCursorPos(ImVec2{ 0, 38 });
							ImGui::BeginChild("##back6", ImVec2{ 100, 4 }, false), TargetFlags1; { //--------------
							}
							ImGui::EndChild();

							if (item.Switch_Page <= 1) {

								ImGui::SetCursorPos(ImVec2{ 4, 46 });
								if (item.Color_Page == 1) {
									Active1();
								}
								else {
									Hovered1();
								}
								if (ImGui::Button("FOV Circle", ImVec2{ 92, 28 })) {
									if (item.Color_Page == 1) {
										item.Color_Page = 1;
									}
									else {
										item.Color_Page = 1;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 4, 79 });
								if (item.Color_Page == 2) {
									Active1();
								}
								else {
									Hovered1();
								}
								if (ImGui::Button("Box", ImVec2{ 92, 28 })) {
									if (item.Color_Page == 2) {
										item.Color_Page = 1;
									}
									else {
										item.Color_Page = 2;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 4, 112 });
								if (item.Color_Page == 3) {
									Active1();
								}
								else {
									Hovered1();
								}
								if (ImGui::Button("Corner Box", ImVec2{ 92, 28 })) {
									if (item.Color_Page == 3) {
										item.Color_Page = 1;
									}
									else {
										item.Color_Page = 3;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 4, 145 });
								if (item.Color_Page == 4) {
									Active1();
								}
								else {
									Hovered1();
								}
								if (ImGui::Button("Box Fill", ImVec2{ 92, 28 })) {
									if (item.Color_Page == 4) {
										item.Color_Page = 1;
									}
									else {
										item.Color_Page = 4;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 4, 178 });
								if (item.Color_Page == 5) {
									Active1();
								}
								else {
									Hovered1();
								}
								if (ImGui::Button("Circle", ImVec2{ 92, 28 })) {
									if (item.Color_Page == 5) {
										item.Color_Page = 1;
									}
									else {
										item.Color_Page = 5;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 4, 211 });
								if (item.Color_Page == 6) {
									Active1();
								}
								else {
									Hovered1();
								}
								if (ImGui::Button("Circle Fill", ImVec2{ 92, 28 })) {
									if (item.Color_Page == 6) {
										item.Color_Page = 1;
									}
									else {
										item.Color_Page = 6;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 4, 244 });
								if (item.Color_Page == 7) {
									Active1();
								}
								else {
									Hovered1();
								}
								if (ImGui::Button("Triangle", ImVec2{ 92, 28 })) {
									if (item.Color_Page == 7) {
										item.Color_Page = 1;
									}
									else {
										item.Color_Page = 7;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 4, 277 });
								if (item.Color_Page == 8) {
									Active1();
								}
								else {
									Hovered1();
								}
								if (ImGui::Button("Triangle Filled", ImVec2{ 92, 28 })) {
									if (item.Color_Page == 8) {
										item.Color_Page = 1;
									}
									else {
										item.Color_Page = 8;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 4, 310 });
								if (item.Color_Page == 9) {
									Active1();
								}
								else {
									Hovered1();
								}
								if (ImGui::Button("Head Dot", ImVec2{ 92, 28 })) {
									if (item.Color_Page == 9) {
										item.Color_Page = 1;
									}
									else {
										item.Color_Page = 9;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 4, 343 });
								if (item.Color_Page == 10) {
									Active1();
								}
								else {
									Hovered1();
								}
								if (ImGui::Button("Line", ImVec2{ 92, 28 })) {
									if (item.Color_Page == 10) {
										item.Color_Page = 1;
									}
									else {
										item.Color_Page = 10;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 4, 376 });
								if (item.Color_Page == 11) {
									Active1();
								}
								else {
									Hovered1();
								}
								if (ImGui::Button("Cross Hair", ImVec2{ 92, 28 })) {
									if (item.Color_Page == 11) {
										item.Color_Page = 1;
									}
									else {
										item.Color_Page = 11;
									}
								}

								ImGui::SetCursorPos(ImVec2{ 4, 409 });
								if (item.Color_Page == 12) {
									Active1();
								}
								else {
									Hovered1();
								}
								if (ImGui::Button("Lock Line", ImVec2{ 92, 28 })) {
									if (item.Color_Page == 12) {
										item.Color_Page = 1;
									}
									else {
										item.Color_Page = 12;
									}
								}
							}
						}
						ImGui::EndChild();

					}
					ImGui::EndChild();
				}
			}
			ImGui::End();
		}
	}
	GetKey();

	ImGui::EndFrame();
	D3dDevice->SetRenderState(D3DRS_ZENABLE, false);
	D3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	D3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
	D3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

	if (D3dDevice->BeginScene() >= 0) {
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		D3dDevice->EndScene();
	}
	HRESULT result = D3dDevice->Present(NULL, NULL, NULL, NULL);

	if (result == D3DERR_DEVICELOST && D3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
		ImGui_ImplDX9_InvalidateDeviceObjects();
		D3dDevice->Reset(&d3dpp);
		ImGui_ImplDX9_CreateDeviceObjects();
	}
}

void xInitD3d()
{
	if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &p_Object)))
		exit(3);

	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.BackBufferWidth = Width;
	d3dpp.BackBufferHeight = Height;
	d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
	d3dpp.MultiSampleQuality = D3DMULTISAMPLE_NONE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.hDeviceWindow = Window;
	d3dpp.Windowed = TRUE;

	p_Object->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &D3dDevice);

	IMGUI_CHECKVERSION();

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;

	ImGui_ImplWin32_Init(Window);
	ImGui_ImplDX9_Init(D3dDevice);

	ImGui::StyleColorsClassic();
	ImGuiStyle* style = &ImGui::GetStyle();

	style->WindowPadding = ImVec2(15, 15);
	style->WindowRounding = 5.0f;
	style->FramePadding = ImVec2(5, 5);
	style->FrameRounding = 4.0f;
	style->ItemSpacing = ImVec2(12, 8);
	style->ItemInnerSpacing = ImVec2(8, 6);
	style->IndentSpacing = 25.0f;
	style->ScrollbarSize = 15.0f;
	style->ScrollbarRounding = 9.0f;
	style->GrabMinSize = 5.0f;
	style->GrabRounding = 3.0f;

	ImVec4* colors = style->Colors;

	colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 0.90f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.10f, 1.00f);
	colors[ImGuiCol_ChildWindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.05f, 0.10f, 0.85f);
	colors[ImGuiCol_Border] = ImVec4(0.70f, 0.70f, 0.70f, 0.65f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.00f, 0.00f, 0.01f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.90f, 0.80f, 0.80f, 0.40f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.90f, 0.65f, 0.65f, 0.45f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.83f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.40f, 0.40f, 0.80f, 0.20f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.00f, 0.00f, 0.87f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.01f, 0.01f, 0.02f, 0.80f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.20f, 0.25f, 0.30f, 0.60f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.55f, 0.53f, 0.55f, 0.51f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.91f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.90f, 0.83f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.70f, 0.70f, 0.70f, 0.62f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.30f, 0.30f, 0.30f, 0.84f);
	colors[ImGuiCol_Button] = ImVec4(0.48f, 0.72f, 0.89f, 0.49f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.50f, 0.69f, 0.99f, 0.68f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.30f, 0.69f, 1.00f, 0.53f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.44f, 0.61f, 0.86f, 1.00f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.38f, 0.62f, 0.83f, 1.00f);
	colors[ImGuiCol_Column] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_ColumnHovered] = ImVec4(0.70f, 0.60f, 0.60f, 1.00f);
	colors[ImGuiCol_ColumnActive] = ImVec4(0.90f, 0.70f, 0.70f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.85f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
	colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 0.00f, 1.00f, 0.35f);
	colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

	style->WindowTitleAlign.x = 0.50f;
	style->FrameRounding = 2.0f;


	p_Object->Release();
}

LPCSTR RandomStringx(int len) {
	std::string str = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	std::string newstr;
	std::string builddate = __DATE__;
	std::string buildtime = __TIME__;
	int pos;
	while (newstr.size() != len) {
		pos = ((rand() % (str.size() - 1)));
		newstr += str.substr(pos, 1);
	}
	return newstr.c_str();
}

int main(int argc, const char* argv[]) {
	CreateThread(NULL, NULL, Menuthread, NULL, NULL, NULL);
	DriverHandle = CreateFileW(_xor_(L"\\\\.\\d31usi0n445").c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	while (DriverHandle == INVALID_HANDLE_VALUE) {
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);

		printf(_xor_("                                                                \n").c_str());
		printf(_xor_(" [+] Driver Did Not Start                                       \n").c_str());
		Sleep(1000);
		printf(_xor_(" [+] Load Error #549                                            \n").c_str());
		Sleep(1000);
		printf(_xor_(" [+] Please close Dripware and try again                       \n").c_str());
		Sleep(2000);
		printf(_xor_("                                                                \n").c_str());
		system("CLS");
		printf(_xor_("                                                                \n").c_str());
		printf(_xor_(" [+] 10                                                         \n").c_str());
		Sleep(1000);
		system("CLS");
		printf(_xor_("                                                                \n").c_str());
		printf(_xor_(" [+] 9                                                          \n").c_str());
		Sleep(1000);
		system("CLS");
		printf(_xor_("                                                                \n").c_str());
		printf(_xor_(" [+] 8                                                          \n").c_str());
		Sleep(1000);
		system("CLS");
		printf(_xor_("                                                                \n").c_str());
		printf(_xor_(" [+] 7                                                          \n").c_str());
		Sleep(1000);
		system("CLS");
		printf(_xor_("                                                                \n").c_str());
		printf(_xor_(" [+] 6                                                          \n").c_str());
		Sleep(1000);
		system("CLS");
		printf(_xor_("                                                                \n").c_str());
		printf(_xor_(" [+] 5                                                          \n").c_str());
		Sleep(1000);
		system("CLS");
		printf(_xor_("                                                                \n").c_str());
		printf(_xor_(" [+] 4                                                          \n").c_str());
		Sleep(1000);
		system("CLS");
		printf(_xor_("                                                                \n").c_str());
		printf(_xor_(" [+] 3                                                          \n").c_str());
		Sleep(1000);
		system("CLS");
		printf(_xor_("                                                                \n").c_str());
		printf(_xor_(" [+] 2                                                          \n").c_str());
		Sleep(1000);
		system("CLS");
		printf(_xor_("                                                                \n").c_str());
		printf(_xor_(" [+] 1                                                          \n").c_str());
		exit(0);
	}

	while (hwnd == NULL) {
		SetConsoleTitle(RandomStringx(10));
		DWORD dLastError = GetLastError();
		LPCTSTR strErrorMessage = NULL;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, dLastError, 0,(LPSTR)&strErrorMessage,	0, NULL);
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
		printf(_xor_("                                     Dripware (V1)                                       \n").c_str());
		printf(_xor_("\n").c_str());
		printf(_xor_(" [+] Waiting for FortniteClient-Win64-Shipping.exe\n").c_str());
		Sleep(450);
		system("CLS");

		XorS(wind, "Fortnite  ");
		hwnd = FindWindowA(0, wind.decrypt());
		GetWindowThreadProcessId(hwnd, &processID);

		RECT rect;
		if (GetWindowRect(hwnd, &rect)) {
			info_t Input_Output_Data;
			Input_Output_Data.pid = processID;
			unsigned long int Readed_Bytes_Amount;

			DeviceIoControl(DriverHandle, ctl_base, &Input_Output_Data, sizeof Input_Output_Data, &Input_Output_Data, sizeof Input_Output_Data, &Readed_Bytes_Amount, nullptr);
			base_address = (unsigned long long int)Input_Output_Data.data;
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
			std::printf(_xor_(" Injection Gross:                      %p.                                                       \n").c_str(), (void*)base_address);
			std::printf(_xor_(" Driver:                               %p.                                                       \n").c_str(), (void*)DriverHandle);
			std::printf(_xor_(" hwnd:                                 %p.                                                       \n\n").c_str(), (void*)hwnd);

			xCreateWindow();
			xInitD3d();

			// make content invisible!
			//XGUARD_WIN(SetWindowDisplayAffinity(Window, WDA_MONITOR));

			HANDLE handle = CreateThread(nullptr, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(cache), nullptr, NULL, nullptr);
			CloseHandle(handle);

			xMainLoop();
		}

		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
		printf(_xor_("                                      Dripware (V1)                                         \n").c_str());
		printf(_xor_("\n").c_str());
		printf(_xor_(" [+] Waiting for FortniteClient-Win64-Shipping.exe\n").c_str());
		Sleep(450);
		system("CLS");

		hwnd = FindWindowA(0, wind.decrypt());
		GetWindowThreadProcessId(hwnd, &processID);

		if (GetWindowRect(hwnd, &rect)) {
			info_t Input_Output_Data;
			Input_Output_Data.pid = processID;
			unsigned long int Readed_Bytes_Amount;

			DeviceIoControl(DriverHandle, ctl_base, &Input_Output_Data, sizeof Input_Output_Data, &Input_Output_Data, sizeof Input_Output_Data, &Readed_Bytes_Amount, nullptr);
			base_address = (unsigned long long int)Input_Output_Data.data;

			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
			std::printf(_xor_(" Injection Gross:                      %p.                                                       \n").c_str(), (void*)base_address);
			std::printf(_xor_(" Driver:                               %p.                                                       \n").c_str(), (void*)DriverHandle);
			std::printf(_xor_(" hwnd:                                 %p.                                                       \n\n").c_str(), (void*)hwnd);

			xCreateWindow();
			xInitD3d();

			// make content invisible!
			//XGUARD_WIN(SetWindowDisplayAffinity(Window, WDA_MONITOR));

			HANDLE handle = CreateThread(nullptr, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(cache), nullptr, NULL, nullptr);
			CloseHandle(handle);

			xMainLoop();
		}

		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
		printf(_xor_("                                      Gh2stware Leak By Luksius                                         \n").c_str());
		printf(_xor_("\n").c_str());
		printf(_xor_(" [+] Waiting for FortniteClient-Win64-Shipping.exe\n").c_str());
		Sleep(450);
		system("CLS");

		GetWindowThreadProcessId(hwnd, &processID);

		if (GetWindowRect(hwnd, &rect)) {
			info_t Input_Output_Data;
			Input_Output_Data.pid = processID;
			unsigned long int Readed_Bytes_Amount;

			DeviceIoControl(DriverHandle, ctl_base, &Input_Output_Data, sizeof Input_Output_Data, &Input_Output_Data, sizeof Input_Output_Data, &Readed_Bytes_Amount, nullptr);
			base_address = (unsigned long long int)Input_Output_Data.data;

			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
			std::printf(_xor_(" Injection Gross:                      %p.                                                       \n").c_str(), (void*)base_address);
			std::printf(_xor_(" Driver:                               %p.                                                       \n").c_str(), (void*)DriverHandle);
			std::printf(_xor_(" hwnd:                                 %p.                                                       \n\n").c_str(), (void*)hwnd);

			xCreateWindow();
			xInitD3d();

			// make content invisible!
			//XGUARD_WIN(SetWindowDisplayAffinity(Window, WDA_MONITOR));

			HANDLE handle = CreateThread(nullptr, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(cache), nullptr, NULL, nullptr);
			CloseHandle(handle);

			xMainLoop();
		}

		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
		printf(_xor_("                                      Gh2stware Leak By Luksius                                       \n").c_str());
		printf(_xor_("\n").c_str());
		printf(_xor_(" [+] Waiting for FortniteClient-Win64-Shipping.exe\n").c_str());
		Sleep(450);
		system("CLS");

		GetWindowThreadProcessId(hwnd, &processID);

		if (GetWindowRect(hwnd, &rect)) {
			info_t Input_Output_Data;
			Input_Output_Data.pid = processID;
			unsigned long int Readed_Bytes_Amount;

			DeviceIoControl(DriverHandle, ctl_base, &Input_Output_Data, sizeof Input_Output_Data, &Input_Output_Data, sizeof Input_Output_Data, &Readed_Bytes_Amount, nullptr);
			base_address = (unsigned long long int)Input_Output_Data.data;

			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
			std::printf(_xor_(" Injection Gross:                      %p.                                                       \n").c_str(), (void*)base_address);
			std::printf(_xor_(" Driver:                               %p.                                                       \n").c_str(), (void*)DriverHandle);
			std::printf(_xor_(" hwnd:                                 %p.                                                       \n\n").c_str(), (void*)hwnd);

			xCreateWindow();
			xInitD3d();

			// make content invisible!
			//XGUARD_WIN(SetWindowDisplayAffinity(Window, WDA_MONITOR));

			HANDLE handle = CreateThread(nullptr, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(cache), nullptr, NULL, nullptr);
			CloseHandle(handle);

			xMainLoop();
		}

		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
		printf(_xor_("                                      Gh2stware Leak By Luksius                                         \n").c_str());
		printf(_xor_("\n").c_str());
		printf(_xor_(" [+] Waiting for FortniteClient-Win64-Shipping.exe\n").c_str());
		Sleep(450);
		system("CLS");

		GetWindowThreadProcessId(hwnd, &processID);

		if (GetWindowRect(hwnd, &rect)) {
			info_t Input_Output_Data;
			Input_Output_Data.pid = processID;
			unsigned long int Readed_Bytes_Amount;

			DeviceIoControl(DriverHandle, ctl_base, &Input_Output_Data, sizeof Input_Output_Data, &Input_Output_Data, sizeof Input_Output_Data, &Readed_Bytes_Amount, nullptr);
			base_address = (unsigned long long int)Input_Output_Data.data;

			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
			std::printf(_xor_(" Injection Gross:                      %p.                                                       \n").c_str(), (void*)base_address);
			std::printf(_xor_(" Driver:                               %p.                                                       \n").c_str(), (void*)DriverHandle);
			std::printf(_xor_(" hwnd:                                 %p.                                                       \n\n").c_str(), (void*)hwnd);

			xCreateWindow();
			xInitD3d();

			// make content invisible!
			//XGUARD_WIN(SetWindowDisplayAffinity(Window, WDA_MONITOR));

			HANDLE handle = CreateThread(nullptr, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(cache), nullptr, NULL, nullptr);
			CloseHandle(handle);

			xMainLoop();
		}
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
		printf(_xor_("                                      Gh2stware Leak By Luksius                                         \n").c_str());
		printf(_xor_("\n").c_str());
		printf(_xor_(" [+] Waiting for FortniteClient-Win64-Shipping.exe\n").c_str());
		Sleep(450);
		system("CLS");

		GetWindowThreadProcessId(hwnd, &processID);

		if (GetWindowRect(hwnd, &rect)) {
			info_t Input_Output_Data;
			Input_Output_Data.pid = processID;
			unsigned long int Readed_Bytes_Amount;

			DeviceIoControl(DriverHandle, ctl_base, &Input_Output_Data, sizeof Input_Output_Data, &Input_Output_Data, sizeof Input_Output_Data, &Readed_Bytes_Amount, nullptr);
			base_address = (unsigned long long int)Input_Output_Data.data;

			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
			std::printf(_xor_(" Injection Gross:                      %p.                                                       \n").c_str(), (void*)base_address);
			std::printf(_xor_(" Driver:                               %p.                                                       \n").c_str(), (void*)DriverHandle);
			std::printf(_xor_(" hwnd:                                 %p.                                                       \n\n").c_str(), (void*)hwnd);

			xCreateWindow();
			xInitD3d();

			// make content invisible!
			//XGUARD_WIN(SetWindowDisplayAffinity(Window, WDA_MONITOR));

			HANDLE handle = CreateThread(nullptr, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(cache), nullptr, NULL, nullptr);
			CloseHandle(handle);

			xMainLoop();
		}

		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
		printf(_xor_("                                      Gh2stware Leak By Luksius                                         \n").c_str());
		printf(_xor_("\n").c_str());
		printf(_xor_(" [+] Waiting for FortniteClient-Win64-Shipping.exe\n").c_str());
		Sleep(450);
		system("CLS");

		GetWindowThreadProcessId(hwnd, &processID);

		if (GetWindowRect(hwnd, &rect)) {
			info_t Input_Output_Data;
			Input_Output_Data.pid = processID;
			unsigned long int Readed_Bytes_Amount;

			DeviceIoControl(DriverHandle, ctl_base, &Input_Output_Data, sizeof Input_Output_Data, &Input_Output_Data, sizeof Input_Output_Data, &Readed_Bytes_Amount, nullptr);
			base_address = (unsigned long long int)Input_Output_Data.data;

			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_BLUE);
			std::printf(_xor_(" Injection Gross:                      %p.                                                       \n").c_str(), (void*)base_address);
			std::printf(_xor_(" Driver:                               %p.                                                       \n").c_str(), (void*)DriverHandle);
			std::printf(_xor_(" hwnd:                                 %p.                                                       \n\n").c_str(), (void*)hwnd);

			xCreateWindow();
			xInitD3d();

			// make content invisible!
			//XGUARD_WIN(SetWindowDisplayAffinity(Window, WDA_MONITOR));

			HANDLE handle = CreateThread(nullptr, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(cache), nullptr, NULL, nullptr);
			CloseHandle(handle);

			xMainLoop();
		}

		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
		printf(_xor_("                                      Gh2stware Leak By Luksius                                         \n").c_str());
		printf(_xor_("\n").c_str());
		printf(_xor_(" [+] Waiting for FortniteClient-Win64-Shipping.exe\n").c_str());
		Sleep(450);
		system("CLS");

		GetWindowThreadProcessId(hwnd, &processID);

		if (GetWindowRect(hwnd, &rect)) {
			info_t Input_Output_Data;
			Input_Output_Data.pid = processID;
			unsigned long int Readed_Bytes_Amount;

			DeviceIoControl(DriverHandle, ctl_base, &Input_Output_Data, sizeof Input_Output_Data, &Input_Output_Data, sizeof Input_Output_Data, &Readed_Bytes_Amount, nullptr);
			base_address = (unsigned long long int)Input_Output_Data.data;

			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
			std::printf(_xor_(" Injection Gross:                      %p.                                                       \n").c_str(), (void*)base_address);
			std::printf(_xor_(" Driver:                               %p.                                                       \n").c_str(), (void*)DriverHandle);
			std::printf(_xor_(" hwnd:                                 %p.                                                       \n\n").c_str(), (void*)hwnd);

			xCreateWindow();
			xInitD3d();

			// make content invisible!
			//XGUARD_WIN(SetWindowDisplayAffinity(Window, WDA_MONITOR));

			HANDLE handle = CreateThread(nullptr, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(cache), nullptr, NULL, nullptr);
			CloseHandle(handle);

			xMainLoop();
		}


	}
	return 0;
}

void SetWindowToTarget()
{
	while (true)
	{
		if (hwnd)
		{
			ZeroMemory(&GameRect, sizeof(GameRect));
			GetWindowRect(hwnd, &GameRect);
			Width = GameRect.right - GameRect.left;
			Height = GameRect.bottom - GameRect.top;
			DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);

			if (dwStyle & WS_BORDER)
			{
				GameRect.top += 32;
				Height -= 39;
			}
			ScreenCenterX = Width / 2;
			ScreenCenterY = Height / 2;
			MoveWindow(Window, GameRect.left, GameRect.top, Width, Height, true);
		}
		else
		{
			exit(0);
		}
	}
}

const MARGINS Margin = { -1 };

void xCreateWindow()
{
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)SetWindowToTarget, 0, 0, 0);

	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.cbSize = sizeof(wc);
	wc.lpszClassName = "Gideion";
	wc.lpfnWndProc = WinProc;
	RegisterClassEx(&wc);

	if (hwnd)
	{
		GetClientRect(hwnd, &GameRect);
		POINT xy;
		ClientToScreen(hwnd, &xy);
		GameRect.left = xy.x;
		GameRect.top = xy.y;

		Width = GameRect.right;
		Height = GameRect.bottom;
	}
	else
		exit(2);

	Window = CreateWindowEx(NULL, "Gideion", "Gideion1", WS_POPUP | WS_VISIBLE, 0, 0, Width, Height, 0, 0, 0, 0);

	DwmExtendFrameIntoClientArea(Window, &Margin);
	SetWindowLong(Window, GWL_EXSTYLE, WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_LAYERED);
	ShowWindow(Window, SW_SHOW);
	UpdateWindow(Window);
}

void WriteAngles(float TargetX, float TargetY, float TargetZ) {
	float x = TargetX / 6.666666666666667f;
	float y = TargetY / 6.666666666666667f;
	float z = TargetZ / 6.666666666666667f;
	y = -(y);

	writefloat(PlayerController + 0x418, y);
	writefloat(PlayerController + 0x418 + 0x4, x);
	writefloat(PlayerController + 0x418, z);
}

MSG Message = { NULL };

void xMainLoop()
{
	static RECT old_rc;
	ZeroMemory(&Message, sizeof(MSG));

	while (Message.message != WM_QUIT)
	{
		if (PeekMessage(&Message, Window, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}

		HWND hwnd_active = GetForegroundWindow();

		if (hwnd_active == hwnd) {
			HWND hwndtest = GetWindow(hwnd_active, GW_HWNDPREV);
			SetWindowPos(Window, hwndtest, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}

		if (GetAsyncKeyState(0x23) & 1)
			exit(8);

		RECT rc;
		POINT xy;

		ZeroMemory(&rc, sizeof(RECT));
		ZeroMemory(&xy, sizeof(POINT));
		GetClientRect(hwnd, &rc);
		ClientToScreen(hwnd, &xy);
		rc.left = xy.x;
		rc.top = xy.y;

		ImGuiIO& io = ImGui::GetIO();
		io.ImeWindowHandle = hwnd;
		io.DeltaTime = 1.0f / 60.0f;

		POINT p;
		GetCursorPos(&p);
		io.MousePos.x = p.x - xy.x;
		io.MousePos.y = p.y - xy.y;

		if (GetAsyncKeyState(VK_LBUTTON)) {
			io.MouseDown[0] = true;
			io.MouseClicked[0] = true;
			io.MouseClickedPos[0].x = io.MousePos.x;
			io.MouseClickedPos[0].x = io.MousePos.y;
		}
		else
			io.MouseDown[0] = false;

		if (rc.left != old_rc.left || rc.right != old_rc.right || rc.top != old_rc.top || rc.bottom != old_rc.bottom)
		{
			old_rc = rc;

			Width = rc.right;
			Height = rc.bottom;

			d3dpp.BackBufferWidth = Width;
			d3dpp.BackBufferHeight = Height;
			SetWindowPos(Window, (HWND)0, xy.x, xy.y, Width, Height, SWP_NOREDRAW);
			D3dDevice->Reset(&d3dpp);
		}
		render();
	}
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	DestroyWindow(Window);
}

LRESULT CALLBACK WinProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam))
		return true;

	switch (Message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		exit(4);
		break;
	case WM_SIZE:
		if (D3dDevice != NULL && wParam != SIZE_MINIMIZED)
		{
			ImGui_ImplDX9_InvalidateDeviceObjects();
			d3dpp.BackBufferWidth = LOWORD(lParam);
			d3dpp.BackBufferHeight = HIWORD(lParam);
			HRESULT hr = D3dDevice->Reset(&d3dpp);
			if (hr == D3DERR_INVALIDCALL)
				IM_ASSERT(0);
			ImGui_ImplDX9_CreateDeviceObjects();
		}
		break;
	default:
		return DefWindowProc(hWnd, Message, wParam, lParam);
		break;
	}
	return 0;
}