#include <Windows.h>

#include <d3d12.h>
#pragma comment(lib, "d3d12.lib")

#include <d3dx12.h>

#include <dxgi1_6.h>
#pragma comment(lib, "dxgi.lib")

#include <vector>

#include <string>

#include <DirectXMath.h>

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")


#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#pragma comment(lib, "dinput8.lib")

#include <DirectXTex.h>

#include<wrl.h>


using namespace DirectX;
using namespace Microsoft::WRL;

#pragma comment(lib, "dxguid.lib")

#include <fstream>
#include <sstream>
using namespace std;

#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")

#include "Time.h"

#include "map.h"

#include "Input.h"

const wchar_t winTitle[] = L"ラビリズムNEXT ~ only SPACE game  ~";

//ウィンドウプロシージャの定義
LRESULT CALLBACK WindowProc(
	HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

#pragma region 2D3D共通

//パイプラインセット
struct PipelineSet {
	//パイプラインステートオブジェクト
	ComPtr<ID3D12PipelineState> pipelinestate;
	//ルートシグネチャ
	ComPtr<ID3D12RootSignature> rootsignature;
};

//定数バッファ用データ構造体
struct ConstBufferData {
	XMFLOAT4 color; //色
	XMMATRIX mat; //行列
};

#pragma endregion

#pragma region 3Dオブジェクト関係

//頂点データ構造体
struct Vertex {
	XMFLOAT3 pos; //xyz座標
	XMFLOAT3 normal; //法線ベクトル
	XMFLOAT2 uv; //uv座標
};


//3Dオブジェクト用パイプライン生成
PipelineSet Object3dCreateGraphicsPipeline(ID3D12Device* dev) {
	HRESULT result;

	ComPtr<ID3DBlob> vsBlob; // 頂点シェーダオブジェクト
	ComPtr<ID3DBlob>psBlob; // ピクセルシェーダオブジェクト
	ComPtr<ID3DBlob>errorBlob; // エラーオブジェクト

	//頂点シェーダーの読み込みとコンパイル
	result = D3DCompileFromFile(
		L"Resources/Shader/BasicVS.hlsl",  // シェーダファイル名
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
		"main", "vs_5_0", // エントリーポイント名、シェーダーモデル指定
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
		0,
		&vsBlob, &errorBlob);

	// ピクセルシェーダの読み込みとコンパイル
	result = D3DCompileFromFile(
		L"Resources/Shader/BasicPS.hlsl",   // シェーダファイル名
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
		"main", "ps_5_0", // エントリーポイント名、シェーダーモデル指定
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
		0,
		&psBlob, &errorBlob);

	//エラー表示
	if (FAILED(result)) {
		// errorBlobからエラー内容をstring型にコピー
		std::string errstr;
		errstr.resize(errorBlob->GetBufferSize());

		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			errstr.begin());
		errstr += "\n";
		// エラー内容を出力ウィンドウに表示
		OutputDebugStringA(errstr.c_str());
		exit(1);
	}

	// 頂点レイアウト
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		}
	};	// 1行で書いたほうが見やすい

	// グラフィックスパイプライン設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline{};
	//頂点シェーダー、ピクセルシェーダ
	gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
	gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
	//標準的な設定(背面カリング、塗りつぶし、深度クリッピング有効)
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // 標準設定

	//ラスタライザステート
	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	//レンダ―ターゲットのブレンド設定
	D3D12_RENDER_TARGET_BLEND_DESC& blenddesc = gpipeline.BlendState.RenderTarget[0];
	blenddesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; //標準設定
	blenddesc.BlendEnable = true; //ブレンドを有効にする
	blenddesc.BlendOpAlpha = D3D12_BLEND_OP_ADD; //加算
	blenddesc.SrcBlendAlpha = D3D12_BLEND_ONE; //ソースの値を100%使う
	blenddesc.DestBlendAlpha = D3D12_BLEND_ZERO; //デストの値を0%使う

	//半透明合成
	blenddesc.BlendOp = D3D12_BLEND_OP_ADD; //加算
	blenddesc.SrcBlend = D3D12_BLEND_SRC_ALPHA; //ソースのアルファ値
	blenddesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA; //1.0f-ソースのアルファ値

	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof(inputLayout);
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpipeline.NumRenderTargets = 1; // 描画対象は1つ
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0～255指定のRGBA
	gpipeline.SampleDesc.Count = 1; // 1ピクセルにつき1回サンプリング

	//デプスステンシルステートの設定
	//標準的な設定(深度テストを行う、書き込み許可、深度が小さければ合格)
	gpipeline.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	//デスクリプタテーブルの設定
	CD3DX12_DESCRIPTOR_RANGE descRangeSRV;
	descRangeSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);//t0レジスタ

	//ルートパラメータの設定
	CD3DX12_ROOT_PARAMETER rootparams[2];
	rootparams[0].InitAsConstantBufferView(0); //定数バッファビューとして初期化(b0レジスタ)
	rootparams[1].InitAsDescriptorTable(1, &descRangeSRV); //テクスチャ用

	//テクスチャサンプラーの設定
	CD3DX12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0);

	PipelineSet pipelineSet;

	//ルートシグネチャの設定
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Init_1_0(_countof(rootparams), rootparams, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> rootSigBlob;
	//バージョン自動判定でのシリアライズ
	result = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob);
	//ルートシグネチャの生成
	result = dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&pipelineSet.rootsignature));
	// パイプラインにルートシグネチャをセット
	gpipeline.pRootSignature = pipelineSet.rootsignature.Get();

	//パイプラインステートの生成
	result = dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&pipelineSet.pipelinestate));

	return pipelineSet;
}

// 3Dオブジェクト型
struct Object3d {
	// 定数バッファ
	ComPtr<ID3D12Resource> constBuff;
	// アフィン変換情報
	XMFLOAT3 scale = { 1,1,1 };
	XMFLOAT3 rotation = { 0,0,0 };
	XMFLOAT3 position = { 0,0,0 };
	// ワールド変換行列
	XMMATRIX matWorld;
	// 親オブジェクトへのポインタ
	Object3d* parent = nullptr;
};

// 3Dオブジェクト初期化
void InitializeObject3d(Object3d* object, int index, ID3D12Device* dev, ID3D12DescriptorHeap* descHeap) {
	HRESULT result;

	// 定数バッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),   // アップロード可能
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(ConstBufferData) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&object->constBuff));
}

void UpdateObject3d(Object3d* object, XMMATRIX& matView, XMMATRIX& matProjection) {
	XMMATRIX matScale, matRot, matTrans;

	// スケール、回転、平行移動行列の計算
	matScale = XMMatrixScaling(object->scale.x, object->scale.y, object->scale.z);
	matRot = XMMatrixIdentity();
	matRot *= XMMatrixRotationZ(XMConvertToRadians(object->rotation.z));
	matRot *= XMMatrixRotationX(XMConvertToRadians(object->rotation.x));
	matRot *= XMMatrixRotationY(XMConvertToRadians(object->rotation.y));
	matTrans = XMMatrixTranslation(object->position.x, object->position.y, object->position.z);

	// ワールド行列の合成
	object->matWorld = XMMatrixIdentity(); // 変形をリセット
	object->matWorld *= matScale; // ワールド行列にスケーリングを反映
	object->matWorld *= matRot; // ワールド行列に回転を反映
	object->matWorld *= matTrans; // ワールド行列に平行移動を反映

	// 親オブジェクトがあれば
	if (object->parent != nullptr) {
		// 親オブジェクトのワールド行列を掛ける
		object->matWorld *= object->parent->matWorld;
	}

	// 定数バッファへデータ転送
	ConstBufferData* constMap = nullptr;
	if (SUCCEEDED(object->constBuff->Map(0, nullptr, (void**)&constMap))) {
		constMap->color = XMFLOAT4(1, 1, 1, 1); // RGBA
		constMap->mat = object->matWorld * matView * matProjection;
		object->constBuff->Unmap(0, nullptr);
	}
}

void DrawObject3d(Object3d* object, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* descHeap,
	D3D12_VERTEX_BUFFER_VIEW& vbView, D3D12_INDEX_BUFFER_VIEW& ibView, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandleSRV, std::vector<unsigned short>& indices, UINT numIndices) {
	// デスクリプタヒープの配列
	ID3D12DescriptorHeap* ppHeaps[] = { descHeap };
	cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	// 頂点バッファの設定
	cmdList->IASetVertexBuffers(0, 1, &vbView);
	// インデックスバッファの設定
	cmdList->IASetIndexBuffer(&ibView);

	// 定数バッファビューをセット
	cmdList->SetGraphicsRootConstantBufferView(0, object->constBuff->GetGPUVirtualAddress());
	// シェーダリソースビューをセット
	cmdList->SetGraphicsRootDescriptorTable(1, gpuDescHandleSRV);
	// 描画コマンド
	cmdList->DrawIndexedInstanced(numIndices, 1, 0, 0, 0);
}
#pragma endregion

#pragma region スプライト関係

//スプライト１枚分のデータ
struct  SpriteData {
	ComPtr<ID3D12Resource> vertBuff;	//頂点バッファ
	D3D12_VERTEX_BUFFER_VIEW vbView;	//頂点バッファビュー
	ComPtr<ID3D12Resource> constBuff;	//定数バッファ

	float rotation = 0.0f;				//Z軸回りの回転角
	XMFLOAT3 position = { 0, 0, 0 };	//座標
	XMMATRIX matWorld;					//ワールド行列
	XMFLOAT4 color = { 1, 1, 1, 1 };	//色
	UINT texNumber = 0;					//テクスチャ番号
	XMFLOAT2 size = { 100, 100 };		//大きさ

	XMFLOAT2 anchorpoint = { 0.0f, 0.0f };	//アンカーポイント

	bool isFlipX = false;	//左右反転するか
	bool isFlipY = false;	//上下反転するか

	XMFLOAT2 texLeftTop = { 0, 0 };		//テクスチャの左上座標
	XMFLOAT2 texSize = { 100, 100 };	//テクスチャの切り出しサイズ

	bool isInvisible = false;			//非表示にするか
};

//スプライト用の頂点データ型
struct VertexPosUv {
	XMFLOAT3 pos; //xyz座標
	XMFLOAT2 uv; //uv座標
};

//テクスチャの最大枚数
const int spriteSRVCount = 512;

//スプライトの共通データ
struct SpriteCommon {
	//パイプラインセット
	PipelineSet pipelineSet;

	//射影行列
	XMMATRIX matProjection;

	//テクスチャ用デスクリプタヒープの生成
	ComPtr<ID3D12DescriptorHeap> descHeap;

	//テクスチャリソース（テクスチャバッファ）の配列
	ComPtr<ID3D12Resource> texBuff[spriteSRVCount];
};
//スプライト用パイプライン生成
PipelineSet SpriteCreateGraphicsPipeline(ID3D12Device* dev) {
	HRESULT result;

	ComPtr<ID3DBlob> vsBlob; // 頂点シェーダオブジェクト
	ComPtr<ID3DBlob>psBlob; // ピクセルシェーダオブジェクト
	ComPtr<ID3DBlob>errorBlob; // エラーオブジェクト

	//頂点シェーダーの読み込みとコンパイル
	result = D3DCompileFromFile(
		L"Resources/Shader/SpriteVS.hlsl",  // シェーダファイル名
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
		"main", "vs_5_0", // エントリーポイント名、シェーダーモデル指定
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
		0,
		&vsBlob, &errorBlob);

	// ピクセルシェーダの読み込みとコンパイル
	result = D3DCompileFromFile(
		L"Resources/Shader/SpritePS.hlsl",   // シェーダファイル名
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
		"main", "ps_5_0", // エントリーポイント名、シェーダーモデル指定
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
		0,
		&psBlob, &errorBlob);

	//エラー表示
	if (FAILED(result)) {
		// errorBlobからエラー内容をstring型にコピー
		std::string errstr;
		errstr.resize(errorBlob->GetBufferSize());

		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			errstr.begin());
		errstr += "\n";
		// エラー内容を出力ウィンドウに表示
		OutputDebugStringA(errstr.c_str());
		exit(1);
	}

	// 頂点レイアウト
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		}
	};	// 1行で書いたほうが見やすい

	// グラフィックスパイプライン設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline{};
	//頂点シェーダー、ピクセルシェーダ
	gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
	gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
	//標準的な設定(背面カリング、塗りつぶし、深度クリッピング有効)
	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); //一旦標準値をリセット
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; //背面カリングなし
	//ラスタライザステート
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // 標準設定

	//レンダ―ターゲットのブレンド設定
	D3D12_RENDER_TARGET_BLEND_DESC& blenddesc = gpipeline.BlendState.RenderTarget[0];
	blenddesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; //標準設定
	blenddesc.BlendEnable = true; //ブレンドを有効にする
	blenddesc.BlendOpAlpha = D3D12_BLEND_OP_ADD; //加算
	blenddesc.SrcBlendAlpha = D3D12_BLEND_ONE; //ソースの値を100%使う
	blenddesc.DestBlendAlpha = D3D12_BLEND_ZERO; //デストの値を0%使う

	//半透明合成
	blenddesc.BlendOp = D3D12_BLEND_OP_ADD; //加算
	blenddesc.SrcBlend = D3D12_BLEND_SRC_ALPHA; //ソースのアルファ値
	blenddesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA; //1.0f-ソースのアルファ値
	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof(inputLayout);
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpipeline.NumRenderTargets = 1; // 描画対象は1つ
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0～255指定のRGBA
	gpipeline.SampleDesc.Count = 1; // 1ピクセルにつき1回サンプリング

	//デプスステンシルステートの設定
	//標準的な設定(深度テストを行う、書き込み許可、深度が小さければ合格)
	gpipeline.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); //一旦標準値をリセット
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS; //常に上書きルール
	gpipeline.DepthStencilState.DepthEnable = false; //深度テストをしない
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	//デスクリプタテーブルの設定
	CD3DX12_DESCRIPTOR_RANGE descRangeSRV;
	descRangeSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);//t0レジスタ

	//ルートパラメータの設定
	CD3DX12_ROOT_PARAMETER rootparams[2];
	rootparams[0].InitAsConstantBufferView(0); //定数バッファビューとして初期化(b0レジスタ)
	rootparams[1].InitAsDescriptorTable(1, &descRangeSRV); //テクスチャ用

	//テクスチャサンプラーの設定
	CD3DX12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0);

	PipelineSet pipelineSet;

	//ルートシグネチャの設定
	ComPtr<ID3D12RootSignature> rootsignature;
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Init_1_0(_countof(rootparams), rootparams, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> rootSigBlob;
	//バージョン自動判定でのシリアライズ
	result = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob);
	//ルートシグネチャの生成
	result = dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&pipelineSet.rootsignature));
	// パイプラインにルートシグネチャをセット
	gpipeline.pRootSignature = pipelineSet.rootsignature.Get();

	//パイプラインステートの生成
	ComPtr<ID3D12PipelineState> pipelinestate;
	result = dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&pipelineSet.pipelinestate));

	return pipelineSet;
}
//スプライト共通グラフィックコマンドのセット
void SpriteCommonBeginDraw(ID3D12GraphicsCommandList* cmdList, const SpriteCommon& spriteCommon) {
	//パイプラインとルートシグネチャの設定
	cmdList->SetPipelineState(spriteCommon.pipelineSet.pipelinestate.Get());
	cmdList->SetGraphicsRootSignature(spriteCommon.pipelineSet.rootsignature.Get());

	//プリミティブ形状を設定
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//デスクリプタヒープをセット
	ID3D12DescriptorHeap* ppHeaps[] = { spriteCommon.descHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
}
//スプライト単体描画
void SpriteDraw(const SpriteData& sprite, ID3D12GraphicsCommandList* cmdList, const SpriteCommon& spriteCommon, ID3D12Device* dev) {
	//非表示フラグがtrueなら
	if (sprite.isInvisible) {
		//描画せず抜ける
		return;
	}

	//頂点バッファをセット
	cmdList->IASetVertexBuffers(0, 1, &sprite.vbView);

	//頂点バッファをセット
	cmdList->SetGraphicsRootConstantBufferView(0, sprite.constBuff->GetGPUVirtualAddress());

	//シェーダリソースビューをセット
	cmdList->SetGraphicsRootDescriptorTable(1,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			spriteCommon.descHeap->GetGPUDescriptorHandleForHeapStart(),
			sprite.texNumber,
			dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)));

	//ポリゴンの描画（４頂点で四角形）
	cmdList->DrawInstanced(4, 1, 0, 0);
}
//スプライト共通データ生成
SpriteCommon SpriteCommonCreate(ID3D12Device* dev, int window_width, int window_height) {
	HRESULT result = S_FALSE;

	//新たなスプライト共通データを作成
	SpriteCommon spriteCommon{};

	//スプライト用パイプライン生成
	spriteCommon.pipelineSet = SpriteCreateGraphicsPipeline(dev);

	//並行行列の射影行列生成
	spriteCommon.matProjection = XMMatrixOrthographicOffCenterLH(
		0.0f, (float)window_width, (float)window_height, 0.0f, 0.0f, 1.0f);

	//デスクリプタヒープを生成
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc{};
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NumDescriptors = spriteSRVCount;
	result = dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&spriteCommon.descHeap));

	return spriteCommon;
}
// スプライト単体更新
void SpriteUpdate(SpriteData& sprite, const SpriteCommon& spriteCommon) {
	//ワールド行列の更新
	sprite.matWorld = XMMatrixIdentity();
	//Z軸回転
	sprite.matWorld *= XMMatrixRotationZ(XMConvertToRadians(sprite.rotation));
	//平行移動
	sprite.matWorld *= XMMatrixTranslation(sprite.position.x, sprite.position.y, sprite.position.z);

	//定数バッファの転送
	ConstBufferData* constMap = nullptr;
	HRESULT result = sprite.constBuff->Map(0, nullptr, (void**)&constMap);
	constMap->mat = sprite.matWorld * spriteCommon.matProjection;
	constMap->color = sprite.color;
	sprite.constBuff->Unmap(0, nullptr);
}
// スプライト共通テクスチャ読み込み
void SpriteCommonLoadTexture(SpriteCommon& spriteCommon, UINT texNumber, const wchar_t* filename, ID3D12Device* dev) {
	//異常な番号の指定を検出
	assert(texNumber <= spriteSRVCount - 1);

	HRESULT result;

	//WICテクスチャのロード
	TexMetadata metadate{};
	ScratchImage scratchImg{};
	result = LoadFromWICFile(
		filename, //画像
		WIC_FLAGS_NONE,
		&metadate, scratchImg);
	const Image* img = scratchImg.GetImage(0, 0, 0); //生データ抽出

	//リソース設定
	CD3DX12_RESOURCE_DESC texresDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		metadate.format,
		metadate.width,
		(UINT)metadate.height,
		(UINT16)metadate.arraySize,
		(UINT16)metadate.mipLevels);

	//テクスチャバッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
		D3D12_HEAP_FLAG_NONE,
		&texresDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&spriteCommon.texBuff[texNumber])
	);

	//テクスチャバッファにデータ転送
	result = spriteCommon.texBuff[texNumber]->WriteToSubresource(
		0,
		nullptr, //全領域にコピー
		img->pixels, //元データアドレス
		(UINT)img->rowPitch, //1ラインサイズ
		(UINT)img->slicePitch //全ラインサイズ
	);

	//シェーダリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{}; //設定構造体
	srvDesc.Format = metadate.format; //RGBA
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; //２Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;

	//ヒープのtexnumber番目にシェーダリソースビュー作成
	dev->CreateShaderResourceView(
		spriteCommon.texBuff[texNumber].Get(), //ビューと関連付けるバッファ
		&srvDesc, //テクスチャ設定情報
		CD3DX12_CPU_DESCRIPTOR_HANDLE(spriteCommon.descHeap->GetCPUDescriptorHandleForHeapStart(), texNumber,
			dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
		)
	);
}
// スプライト単体頂点バッファの転送
void SpriteTransferVertexBuffer(const SpriteData& sprite, const SpriteCommon& spriteCommon) {
	HRESULT result = S_FALSE;

	//頂点データ
	VertexPosUv vertices[] = {
		//			u		  v
		{{}, {0.0f, 1.0f}}, //左下
		{{}, {0.0f, 0.0f}}, //左上
		{{}, {1.0f, 1.0f}}, //右下
		{{}, {1.0f, 0.0f}}, //右上
	};

	//左下、左上、右下、右上
	enum { LB, LT, RB, RT };

	float left = (0.0f - sprite.anchorpoint.x) * sprite.size.x;
	float right = (1.0f - sprite.anchorpoint.x) * sprite.size.x;
	float top = (0.0f - sprite.anchorpoint.y) * sprite.size.y;
	float bottom = (1.0f - sprite.anchorpoint.y) * sprite.size.y;

	if (sprite.isFlipX) {
		left = -left;
		right = -right;
	}
	if (sprite.isFlipY) {
		top = -top;
		bottom = -bottom;
	}

	if (spriteCommon.texBuff[sprite.texNumber]) {
		//テクスチャ情報取得
		D3D12_RESOURCE_DESC resDesc = spriteCommon.texBuff[sprite.texNumber]->GetDesc();

		float tex_left = sprite.texLeftTop.x / resDesc.Width;
		float tex_right = (sprite.texLeftTop.x + sprite.texSize.x) / resDesc.Width;
		float tex_top = sprite.texLeftTop.y / resDesc.Height;
		float tex_bottom = (sprite.texLeftTop.y + sprite.texSize.y) / resDesc.Height;

		vertices[LB].uv = { tex_left, tex_bottom };
		vertices[LT].uv = { tex_left, tex_top };
		vertices[RB].uv = { tex_right, tex_bottom };
		vertices[RT].uv = { tex_right, tex_top };
	}

	vertices[LB].pos = { left, bottom, 0.0f }; //左下
	vertices[LT].pos = { left, top, 0.0f }; //左上
	vertices[RB].pos = { right, bottom, 0.0f }; //右下
	vertices[RT].pos = { right, top, 0.0f }; //右上

	//頂点バッファへのデータ転送
	VertexPosUv* vertMap = nullptr;
	result = sprite.vertBuff->Map(0, nullptr, (void**)&vertMap);
	memcpy(vertMap, vertices, sizeof(vertices));
	sprite.vertBuff->Unmap(0, nullptr);
}
//スプライト生成
//@param sizeX : 表示サイズ。0以下なら画像のサイズにする
//@param sizeY : 表示サイズ。0以下なら画像のサイズにする
SpriteData SpriteCreate(ID3D12Device* dev, int window_width, int window_height,
	UINT texNumber, const SpriteCommon& spriteCommon,
	XMFLOAT2 anchorpoint = { 0.5f, 0.5f },
	bool isFlipX = false, bool isFlipY = false,
	float sizeX = 0.f, float sizeY = 0.f) {

	HRESULT result = S_FALSE;

	///新しいスプライトを作る
	SpriteData sprite{};

	//テクスチャ番号をコピー
	sprite.texNumber = texNumber;

	//アンカーポイントをコピー
	sprite.anchorpoint = anchorpoint;

	//反転フラグをコピー
	sprite.isFlipX = isFlipX;
	sprite.isFlipY = isFlipY;

	//頂点データ
	VertexPosUv vertices[4];

	//指定番号が読み込み済みなら
	if (spriteCommon.texBuff[sprite.texNumber]) {
		//テクスチャ番号取得
		D3D12_RESOURCE_DESC resDesc = spriteCommon.texBuff[sprite.texNumber]->GetDesc();

		float spriteSizeX = sizeX, spriteSizeY = sizeY;	//表示されるサイズ格納用変数
		if (sizeX <= 0.f)spriteSizeX = resDesc.Width;	//引数で指定した値が0以下なら画像のサイズにする
		if (sizeY <= 0.f)spriteSizeY = resDesc.Height;	//引数で指定した値が0以下なら画像のサイズにする

		//スプライトの大きさを画像の解像度に合わせる
		sprite.size = { spriteSizeX,spriteSizeX };
		sprite.texSize = { (float)resDesc.Width, (float)resDesc.Height };
	}

	//頂点バッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&sprite.vertBuff));

	//頂点バッファの転送
	SpriteTransferVertexBuffer(sprite, spriteCommon);

	// 頂点バッファビューの作成
	sprite.vbView.BufferLocation = sprite.vertBuff->GetGPUVirtualAddress();
	sprite.vbView.SizeInBytes = sizeof(vertices);
	sprite.vbView.StrideInBytes = sizeof(vertices[0]);

	//定数バッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(ConstBufferData) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&sprite.constBuff)
	);

	//定数バッファ転送
	ConstBufferData* constMap = nullptr;
	result = sprite.constBuff->Map(0, nullptr, (void**)&constMap);
	constMap->color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	//平行投影行列
	constMap->mat = XMMatrixOrthographicOffCenterLH(
		0.0f, window_width, window_height, 0.0f, 0.0f, 1.0f);
	sprite.constBuff->Unmap(0, nullptr);

	return sprite;
}
#pragma endregion

#pragma region 文字表示
// デバッグ文字列クラスの定義
class DebugText {
public:
	static const int maxStrLen = 256;		// 最大文字数
	static const int fontWidth = 9 * 2;		// フォント画像内1文字分の横幅
	static const int fontHeight = 18 * 2;		// フォント画像内1文字分の縦幅
	static const int fontGraphNumX = 14;	// フォント画像内1行分の文字数

	void init(ID3D12Device* dev, int window_width, int window_height, UINT texNum, const SpriteCommon& spriteCommon);

	void makeTextGraph(const SpriteCommon& spriteCommon, const std::string& text, float x, float y, float scale = 1.0f);

	void draw(ID3D12GraphicsCommandList* cmdList, const SpriteCommon& spriteCommon, ID3D12Device* dev);

private: // メンバ変数     
	// スプライトデータ
	std::vector<SpriteData> sprites{};
	// スプライトデータ配列の添え字
	int spriteIndex = 0;

public:
	DebugText() {
		sprites.resize(maxStrLen);
	}
};

void DebugText::init(ID3D12Device* dev, int window_width, int window_height, UINT texNum, const SpriteCommon& spriteCommon) {
	// 全てのスプライトデータについて
	for (int i = 0; i < sprites.size(); i++) {
		// スプライトを生成する
		sprites[i] = SpriteCreate(dev, window_width, window_height, texNum, spriteCommon, { 0,0 });
	}
}

void DebugText::makeTextGraph(const SpriteCommon& spriteCommon, const std::string& text, float x, float y, float scale) {
	// 全ての文字について
	for (int i = 0; i < text.size(); i++) {
		// 最大文字数を超えたら終了
		if (spriteIndex >= maxStrLen) {
			break;
		}

		// 1文字取り出す(※ASCIIコードでしか成り立たない)
		const unsigned char& character = text[i];

		// ASCIIコードの2段分飛ばした番号を計算
		int fontIndex = character - 32;
		if (character >= 0x7f) {
			fontIndex = 0;
		}

		int fontIndexY = fontIndex / fontGraphNumX;
		int fontIndexX = fontIndex % fontGraphNumX;

		// 座標の計算
		sprites[spriteIndex].position = { x + fontWidth * scale * i, y, 0 };
		sprites[spriteIndex].texLeftTop = { (float)fontIndexX * fontWidth, (float)fontIndexY * fontHeight };
		sprites[spriteIndex].texSize = { fontWidth, fontHeight };
		sprites[spriteIndex].size = { fontWidth * scale, fontHeight * scale };
		// 頂点バッファを転送
		SpriteTransferVertexBuffer(sprites[spriteIndex], spriteCommon);
		// スプライトとしての更新
		SpriteUpdate(sprites[spriteIndex], spriteCommon);

		// 今の文字を１つ進める
		spriteIndex++;
	}
}

// まとめて描画
void DebugText::draw(ID3D12GraphicsCommandList* cmdList, const SpriteCommon& spriteCommon, ID3D12Device* dev) {
	// 全文字のスプライトについて
	for (int i = 0; i < spriteIndex; i++) {
		// スプライト描画
		SpriteDraw(sprites[i], cmdList, spriteCommon, dev);
	}

	//カウントをリセット
	spriteIndex = 0;
}
#pragma endregion

#pragma region チャンク
//チャンクヘッダ
struct ChunkHeader {
	char id[4];     //チャンク毎のID
	int32_t size;   //チャンクサイズ
};
//RIFFヘッダチャンク
struct RiffHeader {
	ChunkHeader chunk;  //"RIFF"
	char type[4];       //"WAVE"
};
//FMTチャンク
struct FormatChunk {
	ChunkHeader chunk;  //"fmt"
	WAVEFORMATEX fmt;   //波形フォーマット
};
#pragma endregion

#pragma region 音
//音声データ
struct SoundData {
	//波形フォーマット
	WAVEFORMATEX wfex;
	//バッファの先頭アドレス
	BYTE* pBuffer;
	//バッファのサイズ
	unsigned int bufferSize;

	IXAudio2SourceVoice* pSourceVoice = nullptr;

private:
	bool playFlag = false;
public:
	bool CheckPlay() { return playFlag; }
	void setPlayFlag(bool newPlayFlag) { playFlag = newPlayFlag; }
};
//音声データの読み込み
SoundData SoundLoadWave(const char* filename) {
	HRESULT result;
	//1.ファイルオープン
	//ファイル入力ストリームのインスタンス
	std::ifstream file;
	//.wavファイルをバイナリモードで開く
	file.open(filename, std::ios_base::binary);
	//ファイルオープン失敗を検出する
	assert(file.is_open());

	//2.wavデータ読み込み
	//RIFFヘッダーの読み込み
	RiffHeader riff;
	file.read((char*)&riff, sizeof(riff));
	//ファイルがRIFFかチェック
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
		assert(0);
	}
	//タイプがWAVEかチェック
	if (strncmp(riff.type, "WAVE", 4) != 0) {
		assert(0);
	}
	//Formatチャンクの読み込み
	FormatChunk format = {};
	//チャンクヘッダーの確認
	file.read((char*)&format, sizeof(ChunkHeader));
	if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
		assert(0);
	}
	//チャンク本体の読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char*)&format.fmt, format.chunk.size);
	//Dataチャンクの読み込み
	ChunkHeader data;
	file.read((char*)&data, sizeof(data));
	//JUNKチャンクを検出した場合
	if (strncmp(data.id, "JUNK", 4) == 0) {
		//読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);
		//再読み込み
		file.read((char*)&data, sizeof(data));
	}
	if (strncmp(data.id, "data", 4) != 0) {
		assert(0);
	}
	//Dataチャンクデータの一部（波形データ）の読み込み
	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);

	//3.ファイルクローズ
	//Waveファイルを閉じる
	file.close();

	//4.読み込んだ音声データをreturn
	//retrunするための音声データ
	SoundData soundData = {};

	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
};
//音声データの解放
void SoundUnload(SoundData* soundData) {
	//バッファのメモリを解放
	delete[] soundData->pBuffer;

	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

void SoundStopWave(SoundData& soundData) {
	HRESULT result;
	result = soundData.pSourceVoice->Stop();
	soundData.setPlayFlag(false);
}

//音声再生
void SoundPlayWave(IXAudio2* xAudio2, SoundData& soundData, int loopCount, float volume = 0.2) {
	HRESULT result;

	//波形フォーマットをもとにSourceVoiceの生成
	result = xAudio2->CreateSourceVoice(&soundData.pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(result));

	//再生する波形データの設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.pBuffer;
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;
	buf.LoopCount = loopCount;

	//波形データの再生
	result = soundData.pSourceVoice->SubmitSourceBuffer(&buf);
	result = soundData.pSourceVoice->SetVolume(volume);
	result = soundData.pSourceVoice->Start();
	soundData.setPlayFlag(true);
}
#pragma endregion

#pragma region 3dモデル読み込み

void loadModel(ID3D12Device* dev, std::vector<Vertex>& vertices, std::vector<unsigned short>& indices, const wchar_t* objPath,
	const int window_width, const int window_height,
	ComPtr<ID3D12Resource>& vertBuff, Vertex* vertMap, D3D12_VERTEX_BUFFER_VIEW& vbView,
	ComPtr<ID3D12Resource>& indexBuff, D3D12_INDEX_BUFFER_VIEW& ibView,
	XMMATRIX& matProjection) {

#pragma region ファイル読み込み

	std::ifstream file;

	file.open(objPath);

	if (file.fail()) {
		assert(0);
	}
	vector<XMFLOAT3> positions;	//頂点座標
	vector<XMFLOAT3> normals;	//法線ベクトル
	vector<XMFLOAT2> texcoords;	//テクスチャUV
	//1行ずつ読み込む
	string line;
	while (getline(file, line)) {

		//1行分の文字列をストリームに変換して解析しやすくする
		std::istringstream line_stream(line);

		//半角スペース区切りで行の先頭を文字列を取得
		string key;
		getline(line_stream, key, ' ');

		if (key == "v") {
			//X,Y,Z座標読み込み
			XMFLOAT3 position{};
			line_stream >> position.x;
			line_stream >> position.y;
			line_stream >> position.z;
			//座標データに追加
			positions.emplace_back(position);
		}

		if (key == "vt") {
			//U,V成分読み込み
			XMFLOAT2 texcoord{};
			line_stream >> texcoord.x;
			line_stream >> texcoord.y;
			//V方向反転
			texcoord.y = 1.0f - texcoord.y;
			//テクスチャ座標データに追加
			texcoords.emplace_back(texcoord);
		}

		if (key == "vn") {
			//X,Y,Z成分読み込み
			XMFLOAT3 normal{};
			line_stream >> normal.x;
			line_stream >> normal.y;
			line_stream >> normal.z;
			//法線ベクトルデータに追加
			normals.emplace_back(normal);
		}

		if (key == "f") {
			//半角スペース区切りで行の続きを読み込む
			string index_string;
			while (getline(line_stream, index_string, ' ')) {
				//頂点インデックス1個分の文字列をストリームに変換して解析しやすくする
				std::istringstream index_stream(index_string);
				unsigned short indexPosition, indexNormal, indexTexcoord;
				index_stream >> indexPosition;
				index_stream.seekg(1, ios_base::cur);//スラッシュをとばす
				index_stream >> indexTexcoord;
				index_stream.seekg(1, ios_base::cur);//スラッシュをとばす
				index_stream >> indexNormal;

				//頂点データの追加
				Vertex vertex{};
				vertex.pos = positions[indexPosition - 1];
				vertex.normal = normals[indexNormal - 1];
				vertex.uv = texcoords[indexTexcoord - 1];
				vertices.emplace_back(vertex);
				//インデックスデータの追加
				indices.emplace_back((unsigned short)indices.size());
			}
		}
	}

	file.close();
#pragma endregion ファイル読み込み

	//頂点データ全体のサイズ = 頂点データ一つ分のサイズ * 頂点データの要素数
	UINT sizeVB = static_cast<UINT>(sizeof(Vertex) * vertices.size());

	HRESULT result = S_FALSE;

	//頂点バッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeVB),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff));

	//GPU上のバッファに対応した仮想メモリを取得
	result = vertBuff->Map(0, nullptr, (void**)&vertMap);

	std::copy(vertices.begin(), vertices.end(), vertMap);

	//マップを解除
	vertBuff->Unmap(0, nullptr);

	//頂点バッファビューの作成
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
	vbView.SizeInBytes = sizeVB;
	vbView.StrideInBytes = sizeof(Vertex);

	//インデックスデータ全体のサイズ
	UINT sizeIB = static_cast<UINT>(sizeof(unsigned short) * indices.size());

	//インデックスバッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeIB),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&indexBuff)
	);

	//インデックスバッファビューの作成
	ibView.BufferLocation = indexBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeIB;

	//GPU上のバッファに対応した仮想メモリを取得
	unsigned short* indexMap = nullptr;
	result = indexBuff->Map(0, nullptr, (void**)&indexMap);

	std::copy(indices.begin(), indices.end(), indexMap);
	//繋がりを解除
	indexBuff->Unmap(0, nullptr);


	//射影変換行列(透視投影)
	matProjection = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(60.0f), // 上下画角60度
		(float)window_width / window_height, // アスペクト比（画面横幅 / 画面縦幅）
		0.1f, 1000.0f // 前端、奥端
	);
}

#pragma endregion 3Dモデル読み込み

//# Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	HRESULT result;
	ComPtr<ID3D12Device> dev;
	ComPtr<IDXGIFactory6> dxgiFactory;
	ComPtr<IDXGISwapChain4> swapchain;
	ComPtr<ID3D12CommandAllocator> cmdAllocator;
	ComPtr<ID3D12GraphicsCommandList> cmdList;
	ComPtr<ID3D12CommandQueue> cmdQueue;
	ComPtr<ID3D12DescriptorHeap> rtvHeaps;

#pragma region winAPI

#ifdef _DEBUG
	//デバッグレイヤーをオンに
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
	}
#endif

	// コンソールへの文字出力
	//OutputDebugStringA("Hello,DirectX!!\n");
	// ウィンドウサイズ
	const int window_width = 800;  // 横幅
	const int window_height = 800;  // 縦幅

	WNDCLASSEX w{}; // ウィンドウクラスの設定
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProc; // ウィンドウプロシージャを設定
	w.lpszClassName = winTitle; // ウィンドウクラス名
	w.hInstance = GetModuleHandle(nullptr); // ウィンドウハンドル
	w.hCursor = LoadCursor(NULL, IDC_ARROW); // カーソル指定

	// ウィンドウクラスをOSに登録
	RegisterClassEx(&w);
	// ウィンドウサイズ{ X座標 Y座標 横幅 縦幅 }
	RECT wrc = { 0, 0, window_width, window_height };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); // 自動でサイズ補正

	// ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(w.lpszClassName, // クラス名
		winTitle,         // タイトルバーの文字
		WS_OVERLAPPEDWINDOW ^ WS_MAXIMIZEBOX ^ WS_THICKFRAME | WS_VISIBLE,        // 標準的なウィンドウスタイル
		CW_USEDEFAULT,              // 表示X座標（OSに任せる）
		CW_USEDEFAULT,              // 表示Y座標（OSに任せる）
		wrc.right - wrc.left,       // ウィンドウ横幅
		wrc.bottom - wrc.top,   // ウィンドウ縦幅
		nullptr,                // 親ウィンドウハンドル
		nullptr,                // メニューハンドル

		w.hInstance,            // 呼び出しアプリケーションハンドル
		nullptr);               // オプション

	// ウィンドウ表示
	ShowWindow(hwnd, SW_SHOW);

	MSG msg{};  // メッセージ

#pragma endregion winAPIここまで

// DirectX初期化処理　ここから
#pragma region DirectX初期化処理

	 // ウィンドウクラスを登録解除
	UnregisterClass(w.lpszClassName, w.hInstance);

	// DXGIファクトリーの生成
	result = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
	// アダプターの列挙用
	std::vector<ComPtr<IDXGIAdapter1>> adapters;
	// ここに特定の名前を持つアダプターオブジェクトが入る
	ComPtr<IDXGIAdapter1> tmpAdapter;
	for (int i = 0;
		dxgiFactory->EnumAdapters1(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND;
		i++) {
		adapters.push_back(tmpAdapter); // 動的配列に追加する
	}

	for (int i = 0; i < adapters.size(); i++) {
		DXGI_ADAPTER_DESC1 adesc;
		adapters[i]->GetDesc1(&adesc);  // アダプターの情報を取得

		// ソフトウェアデバイスを回避
		if (adesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			continue;
		}

		std::wstring strDesc = adesc.Description;   // アダプター名
		// Intel UHD Graphics（オンボードグラフィック）を回避
		if (strDesc.find(L"Intel") == std::wstring::npos) {
			tmpAdapter = adapters[i];   // 採用
			break;
		}
	}

	// 対応レベルの配列
	D3D_FEATURE_LEVEL levels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	D3D_FEATURE_LEVEL featureLevel;

	for (int i = 0; i < _countof(levels); i++) {
		// 採用したアダプターでデバイスを生成
		result = D3D12CreateDevice(tmpAdapter.Get(), levels[i], IID_PPV_ARGS(&dev));
		if (result == S_OK) {
			// デバイスを生成できた時点でループを抜ける
			featureLevel = levels[i];
			break;
		}
	}

	// コマンドアロケータを生成
	result = dev->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&cmdAllocator));

	// コマンドリストを生成
	result = dev->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		cmdAllocator.Get(), nullptr,
		IID_PPV_ARGS(&cmdList));

	// 標準設定でコマンドキューを生成
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc{};

	dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&cmdQueue));

	//IDXGISwapChain1のComPtrを用意
	ComPtr<IDXGISwapChain1> swapchain1;

	// 各種設定をしてスワップチェーンを生成
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
	swapchainDesc.Width = window_width;
	swapchainDesc.Height = window_height;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // 色情報の書式
	swapchainDesc.SampleDesc.Count = 1; // マルチサンプルしない
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER; // バックバッファ用
	swapchainDesc.BufferCount = 2;  // バッファ数を２つに設定
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // フリップ後は破棄
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	dxgiFactory->CreateSwapChainForHwnd(
		cmdQueue.Get(),
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		&swapchain1);

	//生成したIDXGISwapChainのオブジェクトをIDXGISwapChain4に変換する
	swapchain1.As(&swapchain);

	// 各種設定をしてデスクリプタヒープを生成
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー
	heapDesc.NumDescriptors = 2;    // 裏表の２つ
	dev->CreateDescriptorHeap(&heapDesc,
		IID_PPV_ARGS(&rtvHeaps));

	// 裏表の２つ分について
	std::vector<ComPtr<ID3D12Resource>> backBuffers(2);
	for (int i = 0; i < 2; i++) {
		// スワップチェーンからバッファを取得
		result = swapchain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i]));

		dev->CreateRenderTargetView(
			backBuffers[i].Get(),
			nullptr,
			CD3DX12_CPU_DESCRIPTOR_HANDLE(
				rtvHeaps->GetCPUDescriptorHandleForHeapStart(),
				i,
				dev->GetDescriptorHandleIncrementSize(heapDesc.Type)
			)
		);
	}

	//深度バッファ
	ComPtr<ID3D12Resource> depthBuffer;
	//深度バッファリソース設定
	CD3DX12_RESOURCE_DESC depthResDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_D32_FLOAT,
		window_width,
		window_height,
		1, 0,
		1, 0,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	//深度バッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, //深度値の書き込みに使用
		&CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0),
		IID_PPV_ARGS(&depthBuffer));

	//深度ビュー用デスクリプタヒープ作成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
	dsvHeapDesc.NumDescriptors = 1; //深度ビューは1つ
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; //デプスステンシルビュー
	ComPtr<ID3D12DescriptorHeap> dsvHeap;
	result = dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));

	//深度ビュー作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; //深度地フォーマット
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dev->CreateDepthStencilView(
		depthBuffer.Get(),
		&dsvDesc,
		dsvHeap->GetCPUDescriptorHandleForHeapStart());

	// フェンスの生成
	ComPtr<ID3D12Fence> fence;
	UINT64 fenceVal = 0;

	result = dev->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

	ComPtr<IDirectInput8> dinput = nullptr;
	result = DirectInput8Create(w.hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8,
		(void**)&dinput, nullptr);

	ComPtr<IDirectInputDevice8> devkeyboard = nullptr;
	result = dinput->CreateDevice(GUID_SysKeyboard, &devkeyboard, NULL);

	result = devkeyboard->SetDataFormat(&c_dfDIKeyboard);

	result = devkeyboard->SetCooperativeLevel(
		hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY
	);
	// DirectX初期化処理　ここまで
#pragma endregion DirecctX初期化ここまで

#pragma region 描画初期化
	//描画初期化処理

	const float mapSide = 5.f;	//マップチップの1マスの一辺の長さ

	std::vector<Vertex> vertices{
		//前
		{{-mapSide / 2.f, -mapSide / 2.f, -mapSide / 2.f}, {}, {0.0f, 1.0f}},//0
		{{-mapSide / 2.f, +mapSide / 2.f, -mapSide / 2.f}, {}, {0.0f, 0.0f}},//1
		{{+mapSide / 2.f, -mapSide / 2.f, -mapSide / 2.f}, {}, {1.0f, 1.0f}},//2
		{{+mapSide / 2.f, +mapSide / 2.f, -mapSide / 2.f}, {}, {1.0f, 0.0f}},//3
		//後
		{{-mapSide / 2.f, -mapSide / 2.f, +mapSide / 2.f}, {}, {0.0f, 1.0f}},//4
		{{-mapSide / 2.f, +mapSide / 2.f, +mapSide / 2.f}, {}, {0.0f, 0.0f}},//5
		{{+mapSide / 2.f, -mapSide / 2.f, +mapSide / 2.f}, {}, {1.0f, 1.0f}},//6
		{{+mapSide / 2.f, +mapSide / 2.f, +mapSide / 2.f}, {}, {1.0f, 0.0f}},//7
		//左
		{{-mapSide / 2.f, -mapSide / 2.f, -mapSide / 2.f}, {}, {0.0f, 1.0f}},//0
		{{-mapSide / 2.f, -mapSide / 2.f, +mapSide / 2.f}, {}, {0.0f, 0.0f}},//4
		{{-mapSide / 2.f, +mapSide / 2.f, -mapSide / 2.f}, {}, {1.0f, 1.0f}},//1
		{{-mapSide / 2.f, +mapSide / 2.f, +mapSide / 2.f}, {}, {1.0f, 0.0f}},//5
		//右
		{{+mapSide / 2.f, -mapSide / 2.f, -mapSide / 2.f}, {}, {0.0f, 1.0f}},//2
		{{+mapSide / 2.f, -mapSide / 2.f, +mapSide / 2.f}, {}, {0.0f, 0.0f}},//6
		{{+mapSide / 2.f, +mapSide / 2.f, -mapSide / 2.f}, {}, {1.0f, 1.0f}},//3
		{{+mapSide / 2.f, +mapSide / 2.f, +mapSide / 2.f}, {}, {1.0f, 0.0f}},//7
		//下
		{{-mapSide / 2.f, -mapSide / 2.f, -mapSide / 2.f}, {}, {0.0f, 1.0f}},//0
		{{+mapSide / 2.f, -mapSide / 2.f, -mapSide / 2.f}, {}, {0.0f, 0.0f}},//2
		{{-mapSide / 2.f, -mapSide / 2.f, +mapSide / 2.f}, {}, {1.0f, 1.0f}},//4
		{{+mapSide / 2.f, -mapSide / 2.f, +mapSide / 2.f}, {}, {1.0f, 0.0f}},//6
		//上
		{{-mapSide / 2.f, +mapSide / 2.f, -mapSide / 2.f}, {}, {0.0f, 1.0f}},//1
		{{+mapSide / 2.f, +mapSide / 2.f, -mapSide / 2.f}, {}, {0.0f, 0.0f}},//3
		{{-mapSide / 2.f, +mapSide / 2.f, +mapSide / 2.f}, {}, {1.0f, 1.0f}},//5
		{{+mapSide / 2.f, +mapSide / 2.f, +mapSide / 2.f}, {}, {1.0f, 0.0f}},//7
	};

	std::vector<unsigned short> indices{
		//前
		0, 1, 2, //三角形1つ目
		2, 1, 3, //三角形2つ目

		//後
		6, 7, 4, //三角形1つ目
		4, 7, 5, //三角形2つ目

		//左
		8, 9, 10, //三角形1つ目
		10, 9, 11, //三角形2つ目

		//右
		12, 14, 13, //三角形1つ目
		13, 14, 15, //三角形2つ目

		//上
		16, 17, 18, //三角形1つ目
		18, 17, 19, //三角形2つ目

		//下
		20, 22, 21, //三角形1つ目
		21, 22, 23, //三角形2つ目
	};

	for (int i = 0; i < indices.size() / 3; i++) {
		//三角形ひとつごとに計算していく

		//三角形のインデックスを取り出して、一時的な変数に入れる
		unsigned short Tempo1 = indices[i * 3 + 0];
		unsigned short Tempo2 = indices[i * 3 + 1];
		unsigned short Tempo3 = indices[i * 3 + 2];

		XMVECTOR p0 = XMLoadFloat3(&vertices[Tempo1].pos);
		XMVECTOR p1 = XMLoadFloat3(&vertices[Tempo2].pos);
		XMVECTOR p2 = XMLoadFloat3(&vertices[Tempo3].pos);

		XMVECTOR v1 = XMVectorSubtract(p1, p0);
		XMVECTOR v2 = XMVectorSubtract(p2, p0);

		XMVECTOR normal = XMVector3Cross(v1, v2);

		normal = XMVector3Normalize(normal);

		XMStoreFloat3(&vertices[Tempo1].normal, normal);
		XMStoreFloat3(&vertices[Tempo2].normal, normal);
		XMStoreFloat3(&vertices[Tempo3].normal, normal);
	}

	// 頂点データ全体のサイズ = 頂点データ一つ分のサイズ * 頂点データの要素数
	UINT sizeVB = static_cast<UINT>(sizeof(Vertex) * vertices.size());

	// 頂点バッファの生成
	ComPtr<ID3D12Resource> vertBuff;
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeVB),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff));

	// GPU上のバッファに対応した仮想メモリを取得
	Vertex* vertMap = nullptr;
	result = vertBuff->Map(0, nullptr, (void**)&vertMap);

	// 全頂点に対して
	for (int i = 0; i < vertices.size(); i++) {
		vertMap[i] = vertices[i];   // 座標をコピー
	}

	// マップを解除
	vertBuff->Unmap(0, nullptr);

	// 頂点バッファビューの作成
	D3D12_VERTEX_BUFFER_VIEW vbView{};
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
	vbView.SizeInBytes = sizeVB;
	vbView.StrideInBytes = sizeof(Vertex);

	//インデックスデータ全体のサイズ
	UINT sizeIB = static_cast<UINT>(sizeof(unsigned short) * indices.size());

	//インデックスバッファの設定
	ComPtr<ID3D12Resource> indexBuff;

	//インデックスバッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeIB),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&indexBuff)
	);

	//インデックスバッファビューの作成
	D3D12_INDEX_BUFFER_VIEW ibView{};
	ibView.BufferLocation = indexBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeIB;

	//GPU上のバッファに対応した仮想メモリを取得
	unsigned short* indexMap = nullptr;
	result = indexBuff->Map(0, nullptr, (void**)&indexMap);

	//全インデックスに対して
	for (int i = 0; i < indices.size(); i++) {
		indexMap[i] = indices[i];//インデックスをコピー
	}
	//繋がりを解除
	indexBuff->Unmap(0, nullptr);


	//射影変換行列(透視投影)
	XMMATRIX matProjection = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(60.0f), // 上下画角60度
		(float)window_width / window_height, // アスペクト比（画面横幅 / 画面縦幅）
		0.1f, 1000.0f // 前端、奥端
	);

	//ビューの変換行列
	XMMATRIX matView;
	XMFLOAT3 eye(0, 0, -70); //始点座標
	XMFLOAT3 target(eye.x, eye.y, 0); //注意点座標
	XMFLOAT3 up(0, 1, 0); //上方向ベクトル
	matView = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));

	float angle = 0.0f; // カメラの回転角    

	const int constantBufferNum = 128; // 定数バッファの最大数

	//定数バッファ用デスクリプターヒープの生成
	ComPtr<ID3D12DescriptorHeap> basicDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc{};
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NumDescriptors = constantBufferNum + 1;
	// 生成
	result = dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));

	SpriteData frontSprite;	//texNumbreはシーン管理も兼ねている
	SpriteCommon frontSpriteCommon = SpriteCommonCreate(dev.Get(), window_width, window_height);
	enum TEX_NUM { TITLE = 1, PLAY, CLEAR };
	SpriteCommonLoadTexture(frontSpriteCommon, TEX_NUM::TITLE, L"Resources/Front_sprite/rabiri_title.png", dev.Get());
	SpriteCommonLoadTexture(frontSpriteCommon, TEX_NUM::PLAY, L"Resources/Front_sprite/play.png", dev.Get());
	SpriteCommonLoadTexture(frontSpriteCommon, TEX_NUM::CLEAR, L"Resources/Front_sprite/clear.png", dev.Get());
	frontSprite = SpriteCreate(dev.Get(), window_width, window_height, rand() % 2, frontSpriteCommon, { 0,0 }, false, false, window_width, window_height);
	frontSprite.position.x = 0.f;
	frontSprite.position.y = 0.f;
	SpriteTransferVertexBuffer(frontSprite, frontSpriteCommon);



	// 3Dオブジェクトの数
	const int OBJECT_NUM = Map::mapNumX * Map::mapNumY;

	//std::vector<std::vector<Object3d>>
	std::vector<std::vector<Object3d>> object3ds(Map::mapNumY, std::vector<Object3d>(Map::mapNumX));

	for (int x = 0; x < Map::mapNumX; x++) {
		for (int y = 0; y < Map::mapNumY; y++) {
			// 初期化
			InitializeObject3d(&object3ds[y][x], x + y, dev.Get(), basicDescHeap.Get());

			//ここから下は親子構造のサンプル
			//先頭以外なら
			if (!(x == 0 && y == 0)) {
				//先頭(左上)のオブジェクトを親オブジェクトとする
				object3ds[y][x].parent = &object3ds[0][0];

				object3ds[y][x].position = { x * mapSide, y * -mapSide, 0.f };	//マップチップ1ブロックの一辺がmapSide
			} else { /*先頭なら*/
				//object3ds[0].position = { -60.f,60.f,10.f };
				object3ds[y][x].position = { 0,0,0 };
			}
			if (Map::map[y][x] == Map::G) {
				object3ds[y][x].rotation.y = -90.f;
				object3ds[y][x].rotation.z = 90.f;
				object3ds[y][x].position.z += mapSide / 2.f;;
				object3ds[y][x].scale = { 2, 2, 2 };

			}
		}
	}

	//todo : プレイヤー描画と方向矢印(Z軸固定ビルボード) <- 方向矢印はプレイヤーの子オブジェクト
	// 3Dオブジェクト(ビルボード)の回転(Z軸固定)もアンカーポイントを設定したい <- 実現できるの?

	//todo : 背景は天球型にすればよいのでは?(大きな球体で包んで、内側に画像を張る)

#pragma region 3Dオブジェクトシェーダー(テクスチャ)
	//WICテクスチャのロード
	TexMetadata metadata{};
	ScratchImage scratchImg{};

	result = LoadFromWICFile(
		L"Resources/wall.png",
		WIC_FLAGS_NONE,
		&metadata, scratchImg);

	const Image* img = scratchImg.GetImage(0, 0, 0);

	//テクスチャバッファのリソース設定
	CD3DX12_RESOURCE_DESC texresDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		metadata.format,
		metadata.width,
		(UINT)metadata.height,
		(UINT16)metadata.arraySize,
		(UINT16)metadata.mipLevels
	);

	//テクスチャバッファの生成
	ComPtr<ID3D12Resource> texBuff = nullptr;
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
		D3D12_HEAP_FLAG_NONE,
		&texresDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texBuff));

	//テクスチャバッファへのデータ転送
	result = texBuff->WriteToSubresource(
		0,
		nullptr,
		img->pixels,
		(UINT)img->rowPitch,
		(UINT)img->slicePitch
	);

	//シェーダーリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	//ヒープの二番目にシェーダーリソースビュー作成
	dev->CreateShaderResourceView(texBuff.Get(),
		&srvDesc,
		CD3DX12_CPU_DESCRIPTOR_HANDLE(basicDescHeap->GetCPUDescriptorHandleForHeapStart(), constantBufferNum, dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
	);
#pragma endregion

	// 3Dオブジェクト用パイプライン生成
	PipelineSet object3dPipelineSet = Object3dCreateGraphicsPipeline(dev.Get());

	//描画初期化処理　ここまで
#pragma endregion 描画初期化ここまで

#pragma region モデル
	//定数バッファ用デスクリプターヒープの生成
	ComPtr<ID3D12DescriptorHeap> playerDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC playerDescHeapDesc{};
	playerDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	playerDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	playerDescHeapDesc.NumDescriptors = constantBufferNum + 1;
	// 生成
	result = dev->CreateDescriptorHeap(&playerDescHeapDesc, IID_PPV_ARGS(&playerDescHeap));

	std::vector<Vertex> playerVertices{};
	std::vector<unsigned short> playerIndices{};
	ComPtr<ID3D12Resource> playerVertBuff;
	Vertex* playerVertMap{};
	D3D12_VERTEX_BUFFER_VIEW playerVbView;
	ComPtr<ID3D12Resource> playerIndexBuff;
	D3D12_INDEX_BUFFER_VIEW playerIbView;
	XMMATRIX playerMatProjection;

	//プレイヤー限定の変数
	const int playerMapXStart = 1, playerMapYStart = 1;
	int playerMapX = playerMapXStart, playerMapY = playerMapYStart;
	bool movableFlag = true;
	//プレイヤー限定の変数ここまで

	loadModel(dev.Get(), playerVertices, playerIndices, L"Resources/onpu/onpu.obj",
		window_width, window_height,
		playerVertBuff, playerVertMap, playerVbView,
		playerIndexBuff, playerIbView, playerMatProjection);

	const unsigned int playerNum = 1;
	std::vector<Object3d> player(playerNum);
	for (unsigned int i = 0; i < playerNum; i++) {
		InitializeObject3d(&player[i], i, dev.Get(), playerDescHeap.Get());
		player[i].scale = { 1.5f, 1.5f, 1.5f };
		player[i].rotation.x = -60.f;
		player[i].position = object3ds[playerMapY][playerMapX].position;
		/*player[i].position.x += mapSide / 5.f;
		player[i].position.y -= mapSide / 5.f;*/
	}

	eye.x = player[0].position.x;
	eye.y = player[0].position.y;
	target.x = player[0].position.x;
	target.y = player[0].position.y;

#pragma region モデルシェーダー(テクスチャ)
	//WICテクスチャのロード
	TexMetadata playerMetadata{};
	ScratchImage playerScratchImg{};

	result = LoadFromWICFile(
		L"Resources/onpu/onpu_Texture.png",
		WIC_FLAGS_NONE,
		&playerMetadata, playerScratchImg);

	const Image* playerImg = playerScratchImg.GetImage(0, 0, 0);

	//テクスチャバッファのリソース設定
	CD3DX12_RESOURCE_DESC playerTexresDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		playerMetadata.format,
		playerMetadata.width,
		(UINT)playerMetadata.height,
		(UINT16)playerMetadata.arraySize,
		(UINT16)playerMetadata.mipLevels
	);

	//テクスチャバッファの生成
	ComPtr<ID3D12Resource> playertexBuff = nullptr;
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
		D3D12_HEAP_FLAG_NONE,
		&playerTexresDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&playertexBuff));

	//テクスチャバッファへのデータ転送
	result = playertexBuff->WriteToSubresource(
		0,
		nullptr,
		playerImg->pixels,
		(UINT)playerImg->rowPitch,
		(UINT)playerImg->slicePitch
	);

	//シェーダーリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC playerSrvDesc{};
	playerSrvDesc.Format = playerMetadata.format;
	playerSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	playerSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	playerSrvDesc.Texture2D.MipLevels = 1;

	//ヒープの二番目にシェーダーリソースビュー作成
	dev->CreateShaderResourceView(playertexBuff.Get(),
		&playerSrvDesc,
		CD3DX12_CPU_DESCRIPTOR_HANDLE(playerDescHeap->GetCPUDescriptorHandleForHeapStart(), constantBufferNum, dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
	);
#pragma endregion モデルシェーダー(テクスチャ)

#pragma endregion モデル
#pragma region 方向三角モデル
	//定数バッファ用デスクリプターヒープの生成
	ComPtr<ID3D12DescriptorHeap> triangleDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC triangleDescHeapDesc{};
	triangleDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	triangleDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	triangleDescHeapDesc.NumDescriptors = constantBufferNum + 1;
	// 生成
	result = dev->CreateDescriptorHeap(&triangleDescHeapDesc, IID_PPV_ARGS(&triangleDescHeap));

	std::vector<Vertex> triangleVertices{};
	std::vector<unsigned short> triangleIndices{};
	ComPtr<ID3D12Resource> triangleVertBuff;
	Vertex* triangleVertMap{};
	D3D12_VERTEX_BUFFER_VIEW triangleVbView;
	ComPtr<ID3D12Resource> triangleIndexBuff;
	D3D12_INDEX_BUFFER_VIEW triangleIbView;
	XMMATRIX triangleMatProjection;


	loadModel(dev.Get(), triangleVertices, triangleIndices, L"Resources/triangle/triangle.obj",
		window_width, window_height,
		triangleVertBuff, triangleVertMap, triangleVbView,
		triangleIndexBuff, triangleIbView, triangleMatProjection);

	const unsigned int triangleNum = 1;
	std::vector<Object3d> triangle(triangleNum);
	for (unsigned int i = 0; i < triangleNum; i++) {
		InitializeObject3d(&triangle[i], i, dev.Get(), triangleDescHeap.Get());
		triangle[i].rotation.x = -90.f;
		//triangle[i].rotation.z = 180.f;
		triangle[i].position = player[0].position;
		triangle[i].position.y -= mapSide * 1.5f;
		triangle[i].position.z -= mapSide;
	}

#pragma region モデルシェーダー(テクスチャ)
	//WICテクスチャのロード
	TexMetadata triangleMetadata{};
	ScratchImage triangleScratchImg{};

	result = LoadFromWICFile(
		L"Resources/triangle/triangle_tex.png",
		WIC_FLAGS_NONE,
		&triangleMetadata, triangleScratchImg);

	const Image* triangleImg = triangleScratchImg.GetImage(0, 0, 0);

	//テクスチャバッファのリソース設定
	CD3DX12_RESOURCE_DESC triangleTexresDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		triangleMetadata.format,
		triangleMetadata.width,
		(UINT)triangleMetadata.height,
		(UINT16)triangleMetadata.arraySize,
		(UINT16)triangleMetadata.mipLevels
	);

	//テクスチャバッファの生成
	ComPtr<ID3D12Resource> triangletexBuff = nullptr;
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
		D3D12_HEAP_FLAG_NONE,
		&triangleTexresDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&triangletexBuff));

	//テクスチャバッファへのデータ転送
	result = triangletexBuff->WriteToSubresource(
		0,
		nullptr,
		triangleImg->pixels,
		(UINT)triangleImg->rowPitch,
		(UINT)triangleImg->slicePitch
	);

	//シェーダーリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC triangleSrvDesc{};
	triangleSrvDesc.Format = triangleMetadata.format;
	triangleSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	triangleSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	triangleSrvDesc.Texture2D.MipLevels = 1;

	//ヒープの二番目にシェーダーリソースビュー作成
	dev->CreateShaderResourceView(triangletexBuff.Get(),
		&triangleSrvDesc,
		CD3DX12_CPU_DESCRIPTOR_HANDLE(triangleDescHeap->GetCPUDescriptorHandleForHeapStart(), constantBufferNum, dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
	);
#pragma endregion モデルシェーダー(テクスチャ)

#pragma endregion 方向三角モデル

#pragma region ゴールモデル
	//定数バッファ用デスクリプターヒープの生成
	ComPtr<ID3D12DescriptorHeap> goalDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC goalDescHeapDesc{};
	goalDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	goalDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	goalDescHeapDesc.NumDescriptors = constantBufferNum + 1;
	// 生成
	result = dev->CreateDescriptorHeap(&goalDescHeapDesc, IID_PPV_ARGS(&goalDescHeap));

	std::vector<Vertex> goalVertices{};
	std::vector<unsigned short> goalIndices{};
	ComPtr<ID3D12Resource> goalVertBuff;
	Vertex* goalVertMap{};
	D3D12_VERTEX_BUFFER_VIEW goalVbView;
	ComPtr<ID3D12Resource> goalIndexBuff;
	D3D12_INDEX_BUFFER_VIEW goalIbView;
	XMMATRIX goalMatProjection;


	loadModel(dev.Get(), goalVertices, goalIndices, L"Resources/flag_rably/flag_rably.obj",
		window_width, window_height,
		goalVertBuff, goalVertMap, goalVbView,
		goalIndexBuff, goalIbView, goalMatProjection);

	const unsigned int goalNum = 1;
	std::vector<Object3d> goal(goalNum);
	for (unsigned int i = 0; i < goalNum; i++) {
		InitializeObject3d(&goal[i], i, dev.Get(), goalDescHeap.Get());
		goal[i].scale = { 1.5f, 1.5f, 1.5f };
		goal[i].rotation.x = -60.f;
		//goal[i].position = object3ds[goalMapY][goalMapX].position;
		/*goal[i].position.x += mapSide / 5.f;
		goal[i].position.y -= mapSide / 5.f;*/
	}

#pragma region モデルシェーダー(テクスチャ)
	//WICテクスチャのロード
	TexMetadata goalMetadata{};
	ScratchImage goalScratchImg{};

	result = LoadFromWICFile(
		L"Resources/flag_rably/flag_rably.png",
		WIC_FLAGS_NONE,
		&goalMetadata, goalScratchImg);

	const Image* goalImg = goalScratchImg.GetImage(0, 0, 0);

	//テクスチャバッファのリソース設定
	CD3DX12_RESOURCE_DESC goalTexresDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		goalMetadata.format,
		goalMetadata.width,
		(UINT)goalMetadata.height,
		(UINT16)goalMetadata.arraySize,
		(UINT16)goalMetadata.mipLevels
	);

	//テクスチャバッファの生成
	ComPtr<ID3D12Resource> goaltexBuff = nullptr;
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
		D3D12_HEAP_FLAG_NONE,
		&goalTexresDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&goaltexBuff));

	//テクスチャバッファへのデータ転送
	result = goaltexBuff->WriteToSubresource(
		0,
		nullptr,
		goalImg->pixels,
		(UINT)goalImg->rowPitch,
		(UINT)goalImg->slicePitch
	);

	//シェーダーリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC goalSrvDesc{};
	goalSrvDesc.Format = goalMetadata.format;
	goalSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	goalSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	goalSrvDesc.Texture2D.MipLevels = 1;

	//ヒープの二番目にシェーダーリソースビュー作成
	dev->CreateShaderResourceView(goaltexBuff.Get(),
		&goalSrvDesc,
		CD3DX12_CPU_DESCRIPTOR_HANDLE(goalDescHeap->GetCPUDescriptorHandleForHeapStart(), constantBufferNum, dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
	);
#pragma endregion モデルシェーダー(テクスチャ)

#pragma endregion ゴールモデル

#pragma region エフェクトモデル
	//定数バッファ用デスクリプターヒープの生成
	ComPtr<ID3D12DescriptorHeap> effDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC effDescHeapDesc{};
	effDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	effDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	effDescHeapDesc.NumDescriptors = constantBufferNum + 1;
	// 生成
	result = dev->CreateDescriptorHeap(&effDescHeapDesc, IID_PPV_ARGS(&effDescHeap));

	std::vector<Vertex> effVertices{};
	std::vector<unsigned short> effIndices{};
	ComPtr<ID3D12Resource> effVertBuff;
	Vertex* effVertMap{};
	D3D12_VERTEX_BUFFER_VIEW effVbView;
	ComPtr<ID3D12Resource> effIndexBuff;
	D3D12_INDEX_BUFFER_VIEW effIbView;
	XMMATRIX effMatProjection;
	int effEndTime = 500;
	int effR = mapSide * 4;

	float effGravity = 128.f;
	float effSpeed = 1.f;

	int effStartTime = 0;
	int effNowTime = 0;

	bool effFlag = false;

	const float effScale = 0.5f;

	struct effGrain {
		XMFLOAT3 startPos = { 0,0,0 };
		XMFLOAT3 v0 = { 1,1,1 };
		bool alive = false;
		Object3d eff;
	};


	loadModel(dev.Get(), effVertices, effIndices, L"Resources/eff/eff.obj",
		window_width, window_height,
		effVertBuff, effVertMap, effVbView,
		effIndexBuff, effIbView, effMatProjection);

	const unsigned int effNum = 32;
	std::vector<effGrain> grain(effNum);
	for (unsigned int i = 0; i < effNum; i++) {
		InitializeObject3d(&grain[i].eff, i, dev.Get(), effDescHeap.Get());
		grain[i].eff.scale = { effScale, effScale, effScale };
		grain[i].eff.position = player[0].position;
	}

#pragma region モデルシェーダー(テクスチャ)
	//WICテクスチャのロード
	TexMetadata effMetadata{};
	ScratchImage effScratchImg{};

	result = LoadFromWICFile(
		L"Resources/eff/eff.png",
		WIC_FLAGS_NONE,
		&effMetadata, effScratchImg);

	const Image* effImg = effScratchImg.GetImage(0, 0, 0);

	//テクスチャバッファのリソース設定
	CD3DX12_RESOURCE_DESC effTexresDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		effMetadata.format,
		effMetadata.width,
		(UINT)effMetadata.height,
		(UINT16)effMetadata.arraySize,
		(UINT16)effMetadata.mipLevels
	);

	//テクスチャバッファの生成
	ComPtr<ID3D12Resource> efftexBuff = nullptr;
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
		D3D12_HEAP_FLAG_NONE,
		&effTexresDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&efftexBuff));

	//テクスチャバッファへのデータ転送
	result = efftexBuff->WriteToSubresource(
		0,
		nullptr,
		effImg->pixels,
		(UINT)effImg->rowPitch,
		(UINT)effImg->slicePitch
	);

	//シェーダーリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC effSrvDesc{};
	effSrvDesc.Format = effMetadata.format;
	effSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	effSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	effSrvDesc.Texture2D.MipLevels = 1;

	//ヒープの二番目にシェーダーリソースビュー作成
	dev->CreateShaderResourceView(efftexBuff.Get(),
		&effSrvDesc,
		CD3DX12_CPU_DESCRIPTOR_HANDLE(effDescHeap->GetCPUDescriptorHandleForHeapStart(), constantBufferNum, dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
	);
#pragma endregion モデルシェーダー(テクスチャ)

#pragma endregion エフェクトモデル


#pragma region ゲームループ前変数宣言
	//テキスト
	SpriteCommon textCommon = SpriteCommonCreate(dev.Get(), window_width, window_height);
	DebugText text;
	const int textTexNum = 0;
	SpriteCommonLoadTexture(
		textCommon, textTexNum,
		L"Resources/debugfont.png", dev.Get()
	);
	text.init(dev.Get(),
		window_width, window_height,
		textTexNum, textCommon
	);

	const float obj3dMoveVal = mapSide / 2.f;
	const float spriteMoveVal = obj3dMoveVal;

	//時間
	Time* time = new Time(149.99f);
	int changeDirectionNum = 0;
	enum DIRECTION { UP, RIGHT, DOWN, LEFT };
	int nowDirection = DIRECTION::UP;

	int directionChangeTime = 0;

	int clearTime = 99999;

	XMFLOAT3 eyeDef = eye;
	XMFLOAT3 targetDef = target;
	XMFLOAT3 upDef = up;
	enum VIEW_POINT { FP = 1, LOOKDOWN, TP };
	short int viewPoint = VIEW_POINT::LOOKDOWN;

	//--------------------
	//音
	//--------------------
	ComPtr<IXAudio2> xAudio2;
	//XAudioエンジンのインスタンスを作成
	result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	IXAudio2MasteringVoice* masterVoice;
	//マスターボイスを生成
	result = xAudio2->CreateMasteringVoice(&masterVoice);

	//音声読み込み
	SoundData bgm = SoundLoadWave("Resources/BGM/bgm.wav");
	SoundData moveSe = SoundLoadWave("Resources/SE/move.wav");
	SoundData changeDirectionSe = SoundLoadWave("Resources/SE/changeDirection.wav");
	SoundData sceneChangeSe = SoundLoadWave("Resources/SE/sceneChange.wav");


	//最後に動いた方向
	int lastMoveDirection = DIRECTION::DOWN;
	const int lastMoveDirectionStart = lastMoveDirection;


	Input* input = new Input(w.hInstance, hwnd);

	const unsigned int timeStrLen = 64;
	char timeStr[timeStrLen];

#pragma endregion

	// ゲームループ
	while (true) {

#pragma region ループ内システムコマンド
		// メッセージがある？
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg); // キー入力メッセージの処理
			DispatchMessage(&msg); // プロシージャにメッセージを送る
		}

		// ×ボタンで終了メッセージが来たらゲームループを抜ける
		if (msg.message == WM_QUIT) {
			break;
		}
#pragma endregion
#pragma region DirectX毎フレーム処理
		// DirectX毎フレーム処理　ここから

		input->Update();
		if (input->PushKey(DIK_ESCAPE)) break;	//ESCを押したら即終了する

		switch (frontSprite.texNumber) {
		case TEX_NUM::TITLE:
			text.makeTextGraph(textCommon, "Press SPACE...", window_width / 3, window_height - 48);
			if (input->TriggerKey(DIK_SPACE)) {
				frontSprite.texNumber = TEX_NUM::PLAY;

				changeDirectionNum = 0;
				nowDirection = DIRECTION::UP;

				playerMapY = playerMapYStart;
				playerMapX = playerMapXStart;
				player[0].position = object3ds[playerMapY][playerMapX].position;

				eye.x = player[0].position.x;
				eye.y = player[0].position.y;
				target.x = player[0].position.x;
				target.y = player[0].position.y;

				viewPoint = VIEW_POINT::LOOKDOWN;

				lastMoveDirection = lastMoveDirectionStart;
				directionChangeTime = 0;

				triangle[0].position = player[0].position;
				triangle[0].position.z -= mapSide;
				triangle[0].position.y += mapSide * 1.5;
				triangle[0].scale = { 1,1,1 };

				//音流す
				SoundPlayWave(xAudio2.Get(), bgm, XAUDIO2_LOOP_INFINITE);

				//時間計る
				time->init();	//PLAYシーンが始まる瞬間の時間を記録
			}
			break;

		case TEX_NUM::PLAY:
			time->update();	//今の時間を記録

			if (time->getNowTime() >= time->getOneBeatTime() * (changeDirectionNum + 1)) {	//方向変える時間になったら
				directionChangeTime = time->getNowTime();
				changeDirectionNum++;
				nowDirection++;
				movableFlag = true;
				triangle[0].scale = { 1,1,1 };
				SoundPlayWave(xAudio2.Get(), changeDirectionSe, 0);
				if (nowDirection > LEFT) {
					nowDirection = UP;
				}

				triangle[0].position = player[0].position;
				triangle[0].position.z -= mapSide;
				switch (nowDirection) {
				case DIRECTION::UP:
					triangle[0].position.y += mapSide * 1.5;
					break;
				case DIRECTION::RIGHT:
					triangle[0].position.x += mapSide * 1.5;
					break;
				case DIRECTION::DOWN:
					triangle[0].position.y -= mapSide * 1.5;
					break;
				case DIRECTION::LEFT:
					triangle[0].position.x -= mapSide * 1.5;
					break;
				}
			}




#ifdef EYE_MOVE_OK
#pragma region 視点切り替え

			if (viewPoint != VIEW_POINT::FP && input->TriggerKey(DIK_1)) {
				up = { 0,0,-1 };

				eye = player[0].position;

				//視線ベクトルを変更
				target = eye;
				switch (lastMoveDirection) {
				case DIRECTION::UP:
					target.y++;
					break;
				case DIRECTION::RIGHT:
					target.x++;
					break;
				case DIRECTION::DOWN:
					target.y--;
					break;
				case DIRECTION::LEFT:
					target.x--;
					break;
				}
				viewPoint = VIEW_POINT::FP;
			} else if (viewPoint != VIEW_POINT::TP && input->TriggerKey(DIK_3)) {
				up = { 0,0,-1 };

				eye = player[0].position;
				eye.z = -mapSide;

				//視線ベクトルを変更
				target = eye;
				switch (lastMoveDirection) {
				case DIRECTION::UP:
					target.y++;
					break;
				case DIRECTION::RIGHT:
					target.x++;
					break;
				case DIRECTION::DOWN:
					target.y--;
					break;
				case DIRECTION::LEFT:
					target.x--;
					break;
				}
				viewPoint = VIEW_POINT::TP;
			} else if (viewPoint != VIEW_POINT::LOOKDOWN && input->TriggerKey(DIK_2)) {
				eye = eyeDef;
				target = targetDef;
				up = upDef;
				viewPoint = VIEW_POINT::LOOKDOWN;
			}
#pragma endregion 視点切り替え
#endif

			if (movableFlag == true && input->TriggerKey(DIK_SPACE)) {
				//todo 移動
				switch (nowDirection) {
				case DIRECTION::UP:
					if (Map::map[playerMapY - 1][playerMapX] != Map::W) {
						playerMapY--;
						triangle[0].position.y += mapSide;
						movableFlag = false;
					}
					break;
				case DIRECTION::RIGHT:
					if (Map::map[playerMapY][playerMapX + 1] != Map::W) {
						playerMapX++;
						triangle[0].position.x += mapSide;
						movableFlag = false;
					}
					break;
				case DIRECTION::DOWN:
					if (Map::map[playerMapY + 1][playerMapX] != Map::W) {
						playerMapY++;
						triangle[0].position.y -= mapSide;
						movableFlag = false;
					}
					break;
				case DIRECTION::LEFT:
					if (Map::map[playerMapY][playerMapX - 1] != Map::W) {
						playerMapX--;
						triangle[0].position.x -= mapSide;
						movableFlag = false;
					}
					break;
				}
				if (movableFlag == false) {	/*移動していたら*/
					SoundPlayWave(xAudio2.Get(), moveSe, 0);
					lastMoveDirection = nowDirection;

					//プレイヤー移動
					player[0].position = object3ds[playerMapY][playerMapX].position;

					//視点も移動
					eye.x = player[0].position.x;
					eye.y = player[0].position.y;
					target.x = player[0].position.x;
					target.y = player[0].position.y;

					//エフェクト開始
					effFlag = true;
					//todo エフェクト初期化処理
					const int vMax = effR / ((float)effEndTime / 1000.f);
					const int vMin = vMax * 0.75f;
					for (unsigned i = 0; i < grain.size(); i++) {
						grain[i].startPos = player[0].position;
						grain[i].eff.position = grain[i].startPos;
						grain[i].alive = true;

						float angle = XM_PI / 180.f * (rand() % 360);
						float angle2 = XM_PI / 180.f * (rand() % 360);
						float v = rand() % (vMax + vMin) + vMin;

						grain[i].v0.x = v * sinf(angle) * cosf(angle2);
						grain[i].v0.y = v * sinf(angle) * sinf(angle2);
						grain[i].v0.z = v * cosf(angle2);
					}
					effStartTime = time->getNowTime();
					effNowTime = effStartTime;


					if (viewPoint != VIEW_POINT::LOOKDOWN) {
						//始点座標を変更
						eye = player[0].position;
						if (viewPoint == VIEW_POINT::FP) {
							eye.z = 0;
						} else {
							eye.z = -mapSide;
						}

						//視線ベクトルを変更
						target = eye;
						switch (lastMoveDirection) {
						case DIRECTION::UP:
							target.y++;
							break;
						case DIRECTION::RIGHT:
							target.x++;
							break;
						case DIRECTION::DOWN:
							target.y--;
							break;
						case DIRECTION::LEFT:
							target.x--;
							break;
						}
					}

				}
			}

			if (effFlag) {
				effNowTime = time->getNowTime() - effStartTime;
				if (effNowTime >= effEndTime) {
					effFlag = false;
					for (unsigned int i = 0; i < grain.size(); i++) {
						grain[i].alive = false;
					}
				} else {
					//エフェクトの移動処理
					const float effRate = (float)effNowTime / effEndTime;
					const float scaleVal = (1.f - effRate) * effScale;
					const float oneSec = 1000.f;
					for (int i = 0; i < grain.size(); i++) {
						grain[i].eff.position.x = grain[i].startPos.x +
							effSpeed * effNowTime / oneSec * grain[i].v0.x;

						grain[i].eff.position.y = grain[i].startPos.y +
							(grain[i].v0.y * effSpeed * effNowTime / oneSec) -
							(effGravity * effSpeed * effNowTime / oneSec * effSpeed * effNowTime / oneSec / 2.f);

						grain[i].eff.position.z = grain[i].startPos.z +
							effSpeed * effNowTime / oneSec * grain[i].v0.z;

						grain[i].eff.scale = { scaleVal,scaleVal,scaleVal };
					}
					/*char tmp[32];
					snprintf(tmp, 32, "%.2f倍\n", scaleVal);
					OutputDebugStringA(tmp);*/
				}
			}

			//triangle(丸)のscaleを、方向変化から時間がたつにつれて小さくする
			{
				float dirTime = ((float)time->getNowTime() - directionChangeTime) / time->getOneBeatTime();
				triangle[0].scale = { 1 - dirTime, 1 - dirTime, 1 - dirTime };
			}


			//「ゴールしたら」を想定した処理
			if (Map::map[playerMapY][playerMapX] == Map::G) {
				clearTime = time->getNowTime();
				SoundStopWave(bgm);
				SoundPlayWave(xAudio2.Get(), sceneChangeSe, 0);
				frontSprite.texNumber = TEX_NUM::CLEAR;
			}


			//カメラへの変更を行列に反映
			matView = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));

			for (unsigned int i = 0; i < triangle.size(); i++) {
				UpdateObject3d(&triangle[i], matView, triangleMatProjection);
			}
			for (unsigned int i = 0; i < player.size(); i++) {
				UpdateObject3d(&player[i], matView, playerMatProjection);
			}
			for (int x = 0; x < Map::mapNumX; x++) {
				for (int y = 0; y < Map::mapNumY; y++) {
					UpdateObject3d(&object3ds[y][x], matView, matProjection);
				}
			}
			for (unsigned int i = 0; i < grain.size(); i++) {
				UpdateObject3d(&grain[i].eff, matView, effMatProjection);
			}


			//時間表示
			snprintf(timeStr, timeStrLen, "%.3f", (float)time->getNowTime() / 1000.f);
			text.makeTextGraph(textCommon, timeStr, 0, 0);

			break;

		case TEX_NUM::CLEAR:
			snprintf(timeStr, timeStrLen, "TIME : %.3f [s]", (float)clearTime / 1000.f);
			text.makeTextGraph(textCommon, timeStr, window_width / 3, window_height / 3 * 2);
			if (input->TriggerKey(DIK_SPACE)) {
				frontSprite.texNumber = TEX_NUM::TITLE;
			}
			break;
		}

		SpriteUpdate(frontSprite, frontSpriteCommon);


		// GPU上のバッファに対応した仮想メモリを取得
		Vertex* vertMap = nullptr;
		result = vertBuff->Map(0, nullptr, (void**)&vertMap);

		// 全頂点に対して
		for (int i = 0; i < vertices.size(); i++) {
			vertMap[i] = vertices[i];   // 座標をコピー
		}
		// マップを解除
		vertBuff->Unmap(0, nullptr);

		//グラフィックスコマンド
		// バックバッファの番号を取得（2つなので0番か1番）
		UINT bbIndex = swapchain->GetCurrentBackBufferIndex();

		// １．リソースバリアで書き込み可能に変更
		cmdList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(backBuffers[bbIndex].Get(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET)
		);

		// ２．描画先指定
	   //レンダ―ターゲッビュー用ディスクリプタヒープのハンドルを取得
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvH = CD3DX12_CPU_DESCRIPTOR_HANDLE(
			rtvHeaps->GetCPUDescriptorHandleForHeapStart(),
			bbIndex,
			dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
		);

		//深度ステンシルビュー用デスクリプタヒープのハンドルを取得
		D3D12_CPU_DESCRIPTOR_HANDLE dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);

		// ３．画面クリア		R		G	B
		float clearColor[] = { 0.35f, 0.4f, 0.4f }; // 何もないところの描画色
		cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// ４．描画コマンドここから
		//パイプラインステートとルートシグネチャの設定
		cmdList->SetPipelineState(object3dPipelineSet.pipelinestate.Get());
		cmdList->SetGraphicsRootSignature(object3dPipelineSet.rootsignature.Get());

		//ビューポート領域の設定
		cmdList->RSSetViewports(1, &CD3DX12_VIEWPORT(0.0f, 0.0f, window_width, window_height));
		//シザー矩形の設定
		cmdList->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, window_width, window_height));
		//プリミティブ形状を設定
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		switch (frontSprite.texNumber) {
		case TITLE:
			break;

		case PLAY:
			//壁
			for (int x = 0; x < Map::mapNumX; x++) {
				for (int y = 0; y < Map::mapNumY; y++) {
					if (Map::map[y][x] == Map::W) {
						DrawObject3d(&object3ds[y][x], cmdList.Get(), basicDescHeap.Get(), vbView, ibView,
							CD3DX12_GPU_DESCRIPTOR_HANDLE(
								basicDescHeap->GetGPUDescriptorHandleForHeapStart(),
								constantBufferNum,
								dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
							),
							indices, indices.size());
					} else if (Map::map[y][x] == Map::G) {	/*ゴール*/
						for (int i = 0; i < goal.size(); i++) {
							DrawObject3d(&object3ds[y][x], cmdList.Get(), goalDescHeap.Get(), goalVbView, goalIbView,
								CD3DX12_GPU_DESCRIPTOR_HANDLE(goalDescHeap->GetGPUDescriptorHandleForHeapStart(), constantBufferNum, dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)),
								goalIndices, goalIndices.size());
						}
					}
				}
			}
			//プレイヤー描画
			for (int i = 0; i < player.size(); i++) {
				DrawObject3d(&player[i], cmdList.Get(), playerDescHeap.Get(), playerVbView, playerIbView,
					CD3DX12_GPU_DESCRIPTOR_HANDLE(playerDescHeap->GetGPUDescriptorHandleForHeapStart(), constantBufferNum, dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)),
					playerIndices, playerIndices.size());
			}
			//方向描画
			for (int i = 0; i < player.size(); i++) {
				DrawObject3d(&triangle[i], cmdList.Get(), triangleDescHeap.Get(), triangleVbView, triangleIbView,
					CD3DX12_GPU_DESCRIPTOR_HANDLE(triangleDescHeap->GetGPUDescriptorHandleForHeapStart(), constantBufferNum, dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)),
					triangleIndices, triangleIndices.size());
			}
			//エフェクト描画
			for (unsigned int i = 0; i < grain.size(); i++) {
				if (grain[i].alive == true) {
					DrawObject3d(&grain[i].eff, cmdList.Get(), effDescHeap.Get(), effVbView, effIbView,
						CD3DX12_GPU_DESCRIPTOR_HANDLE(effDescHeap->GetGPUDescriptorHandleForHeapStart(), constantBufferNum, dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)),
						effIndices, effIndices.size()
					);
				}
			}

			break;

		case CLEAR:
			break;
		}

		//前景スプライト
		SpriteCommonBeginDraw(cmdList.Get(), frontSpriteCommon);
		SpriteDraw(frontSprite, cmdList.Get(), frontSpriteCommon, dev.Get());

		//テキスト
		SpriteCommonBeginDraw(cmdList.Get(), textCommon);
		text.draw(cmdList.Get(), textCommon, dev.Get());


		// ４．描画コマンドここまで

		// ５．リソースバリアを戻す
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffers[bbIndex].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT)
		);

		// 命令のクローズ
		cmdList->Close();
		// コマンドリストの実行
		ID3D12CommandList* cmdLists[] = { cmdList.Get() }; // コマンドリストの配列
		cmdQueue->ExecuteCommandLists(1, cmdLists);

		// コマンドリストの実行完了を待つ
		cmdQueue->Signal(fence.Get(), ++fenceVal);
		if (fence->GetCompletedValue() != fenceVal) {
			HANDLE event = CreateEvent(nullptr, false, false, nullptr);
			fence->SetEventOnCompletion(fenceVal, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}

		cmdAllocator->Reset(); // キューをクリア
		cmdList->Reset(cmdAllocator.Get(), nullptr);  // 再びコマンドリストを貯める準備

		// バッファをフリップ（裏表の入替え）
		swapchain->Present(1, 0);

		// DirectX毎フレーム処理　ここまで
#pragma endregion DirectX毎フレーム処理ここまで
	}
	//delete
	delete time;
	delete input;
	//XAudio解放
	xAudio2.Reset();
	//音声データ解放
	SoundUnload(&bgm);
	SoundUnload(&moveSe);
	SoundUnload(&changeDirectionSe);
	SoundUnload(&sceneChangeSe);

	// ウィンドウクラスを登録解除
	UnregisterClass(w.lpszClassName, w.hInstance);
	return 0;
}