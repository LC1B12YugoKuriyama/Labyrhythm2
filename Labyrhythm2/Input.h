#pragma once
#include <Windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>


class Input
{
public:
	//namespace�ȗ�
	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public://�����o�֐�
	//������
	void Initialize(HINSTANCE hInstance, HWND hwnd);
	//�X�V
	void Update();
	///<summary>
	///�L�[�̉������`�F�b�N
	///</summary>
	///<param name="keyNumber">�L�[�ԍ��iDIK_0���j</param>
	///<returns>������Ă��邩</returns>
	bool PushKey(BYTE keyNumber);
	///<summary>
	///�L�[�̃g���K�[���`�F�b�N
	///</summary>
	///<param name="keyNumber">�L�[�ԍ��iDIK_0���j</param>
	///<returns>�g���K�[��</returns>
	bool TriggerKey(BYTE keyNumber);

	Input(HINSTANCE hInstance, HWND hwnd);
	~Input();

private://�����o�ϐ�
	//�L�[�{�[�h�f�o�C�X
	ComPtr<IDirectInputDevice8> devkeyboard;
	//DirectInput�̃C���X�^���X
	ComPtr<IDirectInput8> dinput;
	//�S�L�[�̏��
	BYTE key[256] = {};
	//�O��̑S�L�[�̏��
	BYTE keyPre[256] = {};
};

