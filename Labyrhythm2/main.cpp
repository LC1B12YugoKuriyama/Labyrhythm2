#include <Windows.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <vector>
#include <string>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <wrl.h>
#include <xaudio2.h>
#include <fstream>
#include <sstream>
#include "Input.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "xaudio2.lib")

using namespace DirectX;
using namespace Microsoft::WRL;
using namespace std;

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

//パイプラインセット
struct PipelineSet
{
    //パイプラインステートオブジェクト
    ComPtr<ID3D12PipelineState> pipelinestate;
    //ルートシグネチャ
    ComPtr<ID3D12RootSignature> rootsignature;
};

//スプライト１枚分のデータ
struct  Sprite
{
    //頂点バッファ
    ComPtr<ID3D12Resource> vertBuff;
    //頂点バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vbView;
    //定数バッファ
    ComPtr<ID3D12Resource> constBuff;

    //Z軸回りの回転角
    float rotation = 0.0f;
    //座標
    XMFLOAT3 position = { 0, 0, 0 };
    //ワールド行列
    XMMATRIX matWorld;
    //色
    XMFLOAT4 color = { 1, 1, 1, 1 };
    //テクスチャ番号
    UINT texNumber = 0;
    //大きさ
    XMFLOAT2 size = { 100, 100 };

    //アンカーポイント
    XMFLOAT2 anchorpoint = { 0.0f, 0.0f };

    //左右反転
    bool isFilpX = false;
    //上下反転
    bool isFilpY = false;

    //テクスチャの左上座標
    XMFLOAT2 texLeftTop = { 0, 0 };
    //テクスチャの切り出しサイズ
    XMFLOAT2 texSize = { 100, 100 };

    //非表示
    bool isInvisible = false;
};

//頂点データ構造体
struct Vertex
{
    XMFLOAT3 pos; //xyz座標
    XMFLOAT3 normal; //法線ベクトル
    XMFLOAT2 uv; //uv座標
};

//定数バッファ用データ構造体
struct ConstBufferData
{
    XMFLOAT4 color; //色
    XMMATRIX mat; //行列
};

//スプライトの頂点データ型
struct VertexPosUv
{
    XMFLOAT3 pos; //xyz座標
    XMFLOAT2 uv; //uv座標
};

//テクスチャの最大枚数
const int spriteSRVCount = 512;

//スプライトの共通データ
struct SpriteCommon
{
    //パイプラインセット
    PipelineSet pipelineSet;

    //射影行列
    XMMATRIX matProjection;

    //テクスチャ用デスクリプタヒープの生成
    ComPtr<ID3D12DescriptorHeap> descHeap;

    //テクスチャリソース（テクスチャバッファ）の配列
    ComPtr<ID3D12Resource> texBuff[spriteSRVCount];
};

//3Dオブジェクト用パイプライン生成
PipelineSet Object3dCreateGraphicsPipeline(ID3D12Device* dev)
{
    HRESULT result;

    ComPtr<ID3DBlob> vsBlob; // 頂点シェーダオブジェクト
    ComPtr<ID3DBlob>psBlob; // ピクセルシェーダオブジェクト
    ComPtr<ID3DBlob>errorBlob; // エラーオブジェクト

    //頂点シェーダーの読み込みとコンパイル
    result = D3DCompileFromFile(
        L"BasicVS.hlsl",  // シェーダファイル名
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
        "main", "vs_5_0", // エントリーポイント名、シェーダーモデル指定
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
        0,
        &vsBlob, &errorBlob);

    // ピクセルシェーダの読み込みとコンパイル
    result = D3DCompileFromFile(
        L"BasicPS.hlsl",   // シェーダファイル名
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
//スプライト用パイプライン生成
PipelineSet SpriteCreateGraphicsPipeline(ID3D12Device* dev)
{
    HRESULT result;

    ComPtr<ID3DBlob> vsBlob; // 頂点シェーダオブジェクト
    ComPtr<ID3DBlob>psBlob; // ピクセルシェーダオブジェクト
    ComPtr<ID3DBlob>errorBlob; // エラーオブジェクト

    //頂点シェーダーの読み込みとコンパイル
    result = D3DCompileFromFile(
        L"SpriteVS.hlsl",  // シェーダファイル名
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
        "main", "vs_5_0", // エントリーポイント名、シェーダーモデル指定
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
        0,
        &vsBlob, &errorBlob);

    // ピクセルシェーダの読み込みとコンパイル
    result = D3DCompileFromFile(
        L"SpritePS.hlsl",   // シェーダファイル名
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
void SpriteCommonBeginDraw(ID3D12GraphicsCommandList* cmdList, const SpriteCommon& spriteCommon)
{
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
void SpriteDraw(const Sprite& sprite, ID3D12GraphicsCommandList* cmdList, const SpriteCommon& spriteCommon, ID3D12Device* dev)
{
    //非表示フラグがtrueなら
    if (sprite.isInvisible)
    {
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
SpriteCommon SpriteCommonCreate(ID3D12Device* dev, int window_width, int window_height)
{
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
//スプライト単体更新
void SpriteUpdate(Sprite& sprite, const SpriteCommon& spriteCommon)
{
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
//スプライト共通テクスチャ読み込み
void SpriteCommonLoadTexture(SpriteCommon& spriteCommon, UINT texNumber, const wchar_t* filename, ID3D12Device* dev)
{
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
//スプライト単体頂点バッファの転送
void SpriteTransferVertexBuffer(const Sprite& sprite, const SpriteCommon& spriteCommon)
{
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

    if (sprite.isFilpX)
    {
        left = -left;
        right = -right;
    }
    if (sprite.isFilpY)
    {
        top = -top;
        bottom = -bottom;
    }

    if (spriteCommon.texBuff[sprite.texNumber])
    {
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
Sprite SpriteCreate(ID3D12Device* dev, int window_width, int window_height, UINT texNumber, const SpriteCommon& spriteCommon, XMFLOAT2 anchorpoint, bool isFilpX, bool isFilpY)
{
    HRESULT result = S_FALSE;

    //新しいスプライトを作る
    Sprite sprite{};

    //頂点データ
    VertexPosUv vertices[] = {
        {{0.0f, 100.0f, 0.0f}, {0.0f, 1.0f}},//左下
        {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},//左上
        {{100.0f, 100.0f, 0.0f}, {1.0f, 1.0f}},//右下
        {{100.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},//右上
    };

    //テクスチャ番号をコピー
    sprite.texNumber = texNumber;

    //頂点バッファの生成
    result = dev->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices)),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&sprite.vertBuff));

    //指定番号が読み込み済みなら
    if (spriteCommon.texBuff[sprite.texNumber])
    {
        //テクスチャ番号取得
        D3D12_RESOURCE_DESC resDesc = spriteCommon.texBuff[sprite.texNumber]->GetDesc();

        //スプライトの大きさを画像の解像度に合わせる
        sprite.size = { (float)resDesc.Width, (float)resDesc.Height };
    }

    //アンカーポイントをコピー
    sprite.anchorpoint = anchorpoint;

    //反転フラグをコピー
    sprite.isFilpX = isFilpX;
    sprite.isFilpY = isFilpY;

    //頂点バッファの転送
    SpriteTransferVertexBuffer(sprite, spriteCommon);

    //頂点バッファへのデータ転送
    VertexPosUv* vertMap = nullptr;
    result = sprite.vertBuff->Map(0, nullptr, (void**)&vertMap);
    memcpy(vertMap, vertices, sizeof(vertices));
    sprite.vertBuff->Unmap(0, nullptr);

    //頂点バッファビューの作成
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
//3Dオブジェクト型
struct Object3d
{
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
//3Dオブジェクト初期化
void InitializeObject3d(Object3d* object, int index, ID3D12Device* dev, ID3D12DescriptorHeap* descHeap)
{
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
//3Dオブジェクト更新
void UpdateObject3d(Object3d* object, XMMATRIX& matView, XMMATRIX& matProjection)
{
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
//3Dオブジェクト描画
void DrawObject3d(Object3d* object, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* descHeap, D3D12_VERTEX_BUFFER_VIEW& vbView, D3D12_INDEX_BUFFER_VIEW& ibView, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandleSRV, UINT numIndices)
{
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
//音声データ
struct SoundData {
    //波形フォーマット
    WAVEFORMATEX wfex;
    //バッファの先頭アドレス
    BYTE* pBuffer;
    //バッファのサイズ
    unsigned int bufferSize;
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
//音声再生
void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData, int loopCount) {
    HRESULT result;

    //波形フォーマットをもとにSourceVoiceの生成
    IXAudio2SourceVoice* pSourceVoice = nullptr;
    result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(result));

    //再生する波形データの設定
    XAUDIO2_BUFFER buf{};
    buf.pAudioData = soundData.pBuffer;
    buf.AudioBytes = soundData.bufferSize;
    buf.Flags = XAUDIO2_END_OF_STREAM;
    buf.LoopCount = loopCount;

    //波形データの再生
    result = pSourceVoice->SubmitSourceBuffer(&buf);
    result = pSourceVoice->Start();
}
//# Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    HRESULT result;
    ComPtr<ID3D12Device> dev;
    ComPtr<IDXGIFactory6> dxgiFactory;
    ComPtr<IDXGISwapChain4> swapchain;
    ComPtr<ID3D12CommandAllocator> cmdAllocator;
    ComPtr<ID3D12GraphicsCommandList> cmdList;
    ComPtr<ID3D12CommandQueue> cmdQueue;
    ComPtr<ID3D12DescriptorHeap> rtvHeaps;
    ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masterVoice;

#ifdef _DEBUG
    //デバッグレイヤーをオンに
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
#endif

    //コンソールへの文字出力
    //OutputDebugStringA("Hello,DirectX!!\n");
    //ウィンドウサイズ
    const int window_width = 1280;  //横幅
    const int window_height = 720;  //縦幅

    WNDCLASSEX w{}; //ウィンドウクラスの設定
    w.cbSize = sizeof(WNDCLASSEX);
    w.lpfnWndProc = (WNDPROC)WindowProc; //ウィンドウプロシージャを設定
    w.lpszClassName = L"Labyrhythm2"; //ウィンドウクラス名
    w.hInstance = GetModuleHandle(nullptr); //ウィンドウハンドル
    w.hCursor = LoadCursor(NULL, IDC_ARROW); //カーソル指定

    //ウィンドウクラスをOSに登録
    RegisterClassEx(&w);
    //ウィンドウサイズ{ X座標 Y座標 横幅 縦幅 }
    RECT wrc = { 0, 0, window_width, window_height };
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); //自動でサイズ補正

    //ウィンドウオブジェクトの生成
    HWND hwnd = CreateWindow(w.lpszClassName, //クラス名
        L"DirectXGame",         //タイトルバーの文字
        WS_OVERLAPPEDWINDOW,        //標準的なウィンドウスタイル
        CW_USEDEFAULT,              //表示X座標（OSに任せる）
        CW_USEDEFAULT,              //表示Y座標（OSに任せる）
        wrc.right - wrc.left,       //ウィンドウ横幅
        wrc.bottom - wrc.top,   //ウィンドウ縦幅
        nullptr,                //親ウィンドウハンドル
        nullptr,                //メニューハンドル

        w.hInstance,            //呼び出しアプリケーションハンドル
        nullptr);               //オプション

    // ウィンドウ表示
    ShowWindow(hwnd, SW_SHOW);

    MSG msg{};  //メッセージ

// DirectX初期化処理　ここから

     //ウィンドウクラスを登録解除
    UnregisterClass(w.lpszClassName, w.hInstance);

    //スプライト
    SpriteCommon spriteCommon;
    const int SPRITES_NUM = 1;
    Sprite sprites[SPRITES_NUM];

    //DXGIファクトリーの生成
    result = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
    //アダプターの列挙用
    std::vector<ComPtr<IDXGIAdapter1>> adapters;
    //ここに特定の名前を持つアダプターオブジェクトが入る
    ComPtr<IDXGIAdapter1> tmpAdapter;
    for (int i = 0;
        dxgiFactory->EnumAdapters1(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND;
        i++)
    {
        adapters.push_back(tmpAdapter); //動的配列に追加する
    }

    for (int i = 0; i < adapters.size(); i++)
    {
        DXGI_ADAPTER_DESC1 adesc;
        adapters[i]->GetDesc1(&adesc);  //アダプターの情報を取得

        //ソフトウェアデバイスを回避
        if (adesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        std::wstring strDesc = adesc.Description;   // アダプター名
        //Intel UHD Graphics（オンボードグラフィック）を回避
        if (strDesc.find(L"Intel") == std::wstring::npos)
        {
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

    for (int i = 0; i < _countof(levels); i++)
    {
        //採用したアダプターでデバイスを生成
        result = D3D12CreateDevice(tmpAdapter.Get(), levels[i], IID_PPV_ARGS(&dev));
        if (result == S_OK)
        {
            //デバイスを生成できた時点でループを抜ける
            featureLevel = levels[i];
            break;
        }
    }

    //コマンドアロケータを生成
    result = dev->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&cmdAllocator));

    //コマンドリストを生成
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

    //各種設定をしてスワップチェーンを生成
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
    swapchainDesc.Width = 1280;
    swapchainDesc.Height = 720;
    swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  //色情報の書式
    swapchainDesc.SampleDesc.Count = 1; //マルチサンプルしない
    swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER; //バックバッファ用
    swapchainDesc.BufferCount = 2;  //バッファ数を２つに設定
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; //フリップ後は破棄
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

    //各種設定をしてデスクリプタヒープを生成
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; //レンダーターゲットビュー
    heapDesc.NumDescriptors = 2;    //裏表の２つ
    dev->CreateDescriptorHeap(&heapDesc,
        IID_PPV_ARGS(&rtvHeaps));

    //裏表の２つ分について
    std::vector<ComPtr<ID3D12Resource>> backBuffers(2);
    for (int i = 0; i < 2; i++)
    {
        //スワップチェーンからバッファを取得
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

    //フェンスの生成
    ComPtr<ID3D12Fence> fence;
    UINT64 fenceVal = 0;

    result = dev->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

    //ポインタ置き場
    Input* input = nullptr;
    //入力の初期化
    input = new Input();
    input->Initialize(w.hInstance, hwnd);

    //XAudioエンジンのインスタンスを作成
    result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    //マスターボイスを生成
    result = xAudio2->CreateMasteringVoice(&masterVoice);
    //DirectX初期化処理　ここまで

    //描画初期化処理
    std::ifstream file;
    static std::vector<Vertex> vertices;
    static std::vector<unsigned short> indices;

    file.open(L"Resources/triangle_mat/triangle_mat.obj");

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

    //頂点データ全体のサイズ = 頂点データ一つ分のサイズ * 頂点データの要素数
    UINT sizeVB = static_cast<UINT>(sizeof(Vertex) * vertices.size());

    //頂点バッファの生成
    ComPtr<ID3D12Resource> vertBuff;
    result = dev->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(sizeVB),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertBuff));

    //GPU上のバッファに対応した仮想メモリを取得
    Vertex* vertMap = nullptr;
    result = vertBuff->Map(0, nullptr, (void**)&vertMap);

    std::copy(vertices.begin(), vertices.end(), vertMap);

    //マップを解除
    vertBuff->Unmap(0, nullptr);

    //頂点バッファビューの作成
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

    //GPU上おバッファに対応した仮想メモリを取得
    unsigned short* indexMap = nullptr;
    result = indexBuff->Map(0, nullptr, (void**)&indexMap);

    std::copy(indices.begin(), indices.end(), indexMap);
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
    XMFLOAT3 eye(0, 0, -100); //始点座標
    XMFLOAT3 tartget(0, 0, 0); //注意点座標
    XMFLOAT3 up(0, 1, 0); //上方向ベクトル
    matView = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&tartget), XMLoadFloat3(&up));

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

    //3Dオブジェクトの数
    const int OBJECT_NUM = 1;

    Object3d object3ds[OBJECT_NUM];

    //配列内の全オブジェクトに対して
    for (int i = 0; i < _countof(object3ds); i++)
    {
        //初期化
        InitializeObject3d(&object3ds[i], i, dev.Get(), basicDescHeap.Get());
    }

    object3ds[0].scale = { 20.0f, 20.0f, 20.0f };

    //スプライト共通データ生成
    spriteCommon = SpriteCommonCreate(dev.Get(), window_width, window_height);
    //スプライト共通テクスチャ読み込み
    SpriteCommonLoadTexture(spriteCommon, 0, L"Resources/player.png", dev.Get());
    //SpriteCommonLoadTexture(spriteCommon, 1, L"Resources/player.png", dev.Get());

    //スプライトの生成
    for (int i = 0; i < _countof(sprites); i++)
    {
        int texNumber = rand() % 2;
        sprites[i] = SpriteCreate(dev.Get(), window_width, window_height, texNumber, spriteCommon, { 0,0 }, false, false);

        //スプライトの座標変更
        sprites[i].position.x = 1280 / 2;
        sprites[i].position.y = 720 / 2;

        //頂点バッファに反映
        SpriteTransferVertexBuffer(sprites[i], spriteCommon);
    }

    //WICテクスチャのロード
    TexMetadata metadata{};
    ScratchImage scratchImg{};

    result = LoadFromWICFile(
        L"Resources/triangle_mat/tex1.png",
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

    // 3Dオブジェクト用パイプライン生成
    PipelineSet object3dPipelineSet = Object3dCreateGraphicsPipeline(dev.Get());
    //音声読み込み
    SoundData soundData1 = SoundLoadWave("Resources/BGM/Alarm01.wav");
    //音声再生

    //描画初期化処理　ここまで

    while (true)  // ゲームループ
    {
        //メッセージがある？
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); // キー入力メッセージの処理
            DispatchMessage(&msg); // プロシージャにメッセージを送る
        }

        //✖ボタンで終了メッセージが来たらゲームループを抜ける
        if (msg.message == WM_QUIT) {
            break;
        }
        //DirectX毎フレーム処理　ここから
        input->Update();

        if (input->TriggerKey(DIK_0)) {
            //OutputDebugStringA("Hit 0\n");
            SoundPlayWave(xAudio2.Get(), soundData1, 255);
        }

        //座標操作
        if (input->PushKey(DIK_UP) || input->PushKey(DIK_DOWN) || input->PushKey(DIK_RIGHT) || input->PushKey(DIK_LEFT))
        {
            if (input->PushKey(DIK_UP)) { object3ds[0].position.y += 1.0f; } else if (input->PushKey(DIK_DOWN)) { object3ds[0].position.y -= 1.0f; }
            if (input->PushKey(DIK_RIGHT)) { object3ds[0].position.x += 1.0f; } else if (input->PushKey(DIK_LEFT)) { object3ds[0].position.x -= 1.0f; }
        }

        for (int i = 0; i < _countof(object3ds); i++)
        {
            UpdateObject3d(&object3ds[i], matView, matProjection);
        }

        //GPU上のバッファに対応した仮想メモリを取得
        Vertex* vertMap = nullptr;
        result = vertBuff->Map(0, nullptr, (void**)&vertMap);

        std::copy(vertices.begin(), vertices.end(), vertMap);

        //マップを解除
        vertBuff->Unmap(0, nullptr);

        //グラフィックスコマンド
        //バックバッファの番号を取得（2つなので0番か1番）
        UINT bbIndex = swapchain->GetCurrentBackBufferIndex();

        //１．リソースバリアで書き込み可能に変更
        cmdList->ResourceBarrier(
            1,
            &CD3DX12_RESOURCE_BARRIER::Transition(backBuffers[bbIndex].Get(),
                D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATE_RENDER_TARGET)
        );

        //２．描画先指定
       //レンダ―ターゲッビュー用ディスクリプタヒープのハンドルを取得
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvH = CD3DX12_CPU_DESCRIPTOR_HANDLE(
            rtvHeaps->GetCPUDescriptorHandleForHeapStart(),
            bbIndex,
            dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
        );

        //深度ステンシルビュー用デスクリプタヒープのハンドルを取得
        D3D12_CPU_DESCRIPTOR_HANDLE dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
        cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);

        //３．画面クリア
        float clearColor[] = { 0.25f, 0.5f, 1.0f }; // 青っぽい色
        cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
        cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        //４．描画コマンドここから
        //パイプラインステートとルートシグネチャの設定
        cmdList->SetPipelineState(object3dPipelineSet.pipelinestate.Get());
        cmdList->SetGraphicsRootSignature(object3dPipelineSet.rootsignature.Get());

        //ビューポート領域の設定
        cmdList->RSSetViewports(1, &CD3DX12_VIEWPORT(0.0f, 0.0f, window_width, window_height));
        //シザー矩形の設定
        cmdList->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, window_width, window_height));
        //プリミティブ形状を設定
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        for (int i = 0; i < _countof(object3ds); i++)
        {
            DrawObject3d(&object3ds[i], cmdList.Get(), basicDescHeap.Get(), vbView, ibView,
                CD3DX12_GPU_DESCRIPTOR_HANDLE(basicDescHeap->GetGPUDescriptorHandleForHeapStart(), constantBufferNum, dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)),
                indices.size());
        }

        //スプライト共通コマンド
        SpriteCommonBeginDraw(cmdList.Get(), spriteCommon);

        //スプライト描画
        for (int i = 0; i < _countof(sprites); i++)
        {
            SpriteDraw(sprites[i], cmdList.Get(), spriteCommon, dev.Get());
        }

        //４．描画コマンドここまで

        //５．リソースバリアを戻す
        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffers[bbIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT)
        );

        //命令のクローズ
        cmdList->Close();
        //コマンドリストの実行
        ID3D12CommandList* cmdLists[] = { cmdList.Get() }; //コマンドリストの配列
        cmdQueue->ExecuteCommandLists(1, cmdLists);

        //コマンドリストの実行完了を待つ
        cmdQueue->Signal(fence.Get(), ++fenceVal);
        if (fence->GetCompletedValue() != fenceVal) {
            HANDLE event = CreateEvent(nullptr, false, false, nullptr);
            fence->SetEventOnCompletion(fenceVal, event);
            WaitForSingleObject(event, INFINITE);
            CloseHandle(event);
        }

        cmdAllocator->Reset(); //キューをクリア
        cmdList->Reset(cmdAllocator.Get(), nullptr);  // 再びコマンドリストを貯める準備

        //バッファをフリップ（裏表の入替え）
        swapchain->Present(1, 0);

        //DirectX毎フレーム処理　ここまで
    }

    //入力解放
    delete input;
    //XAudio解放
    xAudio2.Reset();
    //音声データ解放
    SoundUnload(&soundData1);

    //ウィンドウクラスを登録解除
    UnregisterClass(w.lpszClassName, w.hInstance);
    return 0;
}