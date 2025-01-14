﻿#include "ImguiDrawer.h"

#include "ClearQuad.h"
#include "ImguiShader.h"
#include "ImguiWrap/ImguiResourceManager.h"

TArray<TSharedPtr<FImguiDrawer, ESPMode::ThreadSafe>> FImguiDrawer::GlobalPool;

FImguiDrawer::~FImguiDrawer()
{
	if (VtxBuf)
	{
		FMemory::Free(VtxBuf);
	}
	if (IdxBuf)
	{
		FMemory::Free(IdxBuf);
	}
}

void FImguiDrawer::SetDrawData(ImDrawData* InDrawData)
{
	// get vertex and index num 
	int32 VtxNum = InDrawData->TotalVtxCount;
	int32 IdxNum = InDrawData->TotalIdxCount;

	// update buffer 
	_UpdateBufferSize(VtxNum, IdxNum);

	// copy data 
	_AppendDrawData(InDrawData, VtxBuf, IdxBuf);
}

void FImguiDrawer::SetDrawData(const TArray<ImDrawData*>& InDrawData)
{
	// get vertex and index num 
	int32 VtxNum = 0;
	int32 IdxNum = 0;
	for (ImDrawData* DrawData : InDrawData)
	{
		VtxNum += DrawData->TotalVtxCount;
		IdxNum += DrawData->TotalIdxCount;
	}

	// update buffer
	_UpdateBufferSize(VtxNum, IdxNum);

	// copy data 
	int32 BaseVtx = 0;
	int32 BaseIdx = 0;
	for (ImDrawData* DrawData : InDrawData)
	{
		_AppendDrawData(DrawData, VtxBuf, IdxBuf, BaseVtx, BaseIdx);
		BaseVtx += DrawData->TotalVtxCount;
		BaseIdx += DrawData->TotalIdxCount;
	}
}

void FImguiDrawer::SetDrawData(const TArray<ImDrawList*>& InDrawData)
{
	// reset draw elements 
	AllDrawElements.Reset();
	
	// get vertex and index num 
	int32 VtxNum = 0;
	int32 IdxNum = 0;
	for (ImDrawList* DrawList : InDrawData)
	{
		VtxNum += DrawList->VtxBuffer.Size;
		IdxNum += DrawList->IdxBuffer.Size;
	}

	// update buffer
	_UpdateBufferSize(VtxNum, IdxNum);

	// copy data 
	int32 BaseVtx = 0;
	int32 BaseIdx = 0;
	for (ImDrawList* DrawList : InDrawData)
	{
		_AppendDrawList(DrawList, VtxBuf, IdxBuf, BaseVtx, BaseIdx);
		BaseVtx += DrawList->VtxBuffer.Size;
		BaseIdx += DrawList->IdxBuffer.Size;
	}
}

FVertexDeclarationRHIRef FImguiDrawer::_GetMeshDeclaration()
{
	static FVertexDeclarationRHIRef VertexDeclarationRHI;
	if (!VertexDeclarationRHI.IsValid())
	{
		FVertexDeclarationElementList Elements;
		uint16 Stride = sizeof(ImDrawVert);
		Elements.Add(FVertexElement(0,STRUCT_OFFSET(ImDrawVert,pos),VET_Float2,0,Stride));
		Elements.Add(FVertexElement(0,STRUCT_OFFSET(ImDrawVert,uv),VET_Float2,1,Stride));
		Elements.Add(FVertexElement(0,STRUCT_OFFSET(ImDrawVert,col),VET_UByte4N,2,Stride));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}
	return VertexDeclarationRHI;
}

void FImguiDrawer::_UpdateBufferSize(int32 InVtxNum, int32 InIdxNum)
{
#if ENGINE_MAJOR_VERSION == 5
	FRHIResourceCreateInfo CreateInfo(TEXT("IMGUI_BUFFER"));
#else
	FRHIResourceCreateInfo CreateInfo;
#endif
	if (InVtxNum > NumVertices)
	{
		auto VtxBufSize = sizeof(ImDrawVert) * InVtxNum;
        VertexBufferRHI = FRHICommandListExecutor::GetImmediateCommandList().CreateVertexBuffer(VtxBufSize, BUF_Dynamic, CreateInfo);
		if (VtxBuf)
		{
			FMemory::Free(VtxBuf);
		}
		VtxBuf = (ImDrawVert*)FMemory::Malloc(VtxBufSize);
		NumVertices = InVtxNum;
    }
	if (InIdxNum > NumTriangles * 3)
	{
		auto IdxBufSize = sizeof(ImDrawIdx) * InIdxNum;
		check(InIdxNum % 3 == 0);
		IndexBufferRHI = FRHICommandListExecutor::GetImmediateCommandList().CreateIndexBuffer(sizeof(ImDrawIdx), IdxBufSize, BUF_Dynamic, CreateInfo);
		if (IdxBuf)
		{
			FMemory::Free(IdxBuf);
		}
		IdxBuf = (ImDrawIdx*)FMemory::Malloc(IdxBufSize);		
		NumTriangles = InIdxNum / 3;
	}
}

void FImguiDrawer::_AppendDrawData(ImDrawData* InDrawData, ImDrawVert* InVtxBuf, ImDrawIdx* InIdxBuf, int32 BaseVtx,
	int32 BaseIdx)
{
	// offset vtx and idx 
	InVtxBuf += BaseVtx;
	InIdxBuf += BaseIdx;
	
	uint32 GlobalVtxOffset = BaseVtx;
	uint32 GlobalIdxOffset = BaseIdx;
	for (int32 ListIdx = 0; ListIdx < InDrawData->CmdListsCount; ++ListIdx)
	{ 
		ImDrawList* DrawList = InDrawData->CmdLists[ListIdx];

		_AppendDrawList(DrawList, InVtxBuf, InIdxBuf, GlobalVtxOffset, GlobalIdxOffset);
		
		GlobalVtxOffset += DrawList->VtxBuffer.Size;
		GlobalIdxOffset += DrawList->IdxBuffer.Size;
	}
}

void FImguiDrawer::_AppendDrawList(ImDrawList* InDrawList, ImDrawVert* InVtxBuf, ImDrawIdx* InIdxBuf, int32 BaseVtx,
	int32 BaseIdx)
{
	// offset vtx and idx 
	InVtxBuf += BaseVtx;
	InIdxBuf += BaseIdx;

	// copy vertices and indices
	FMemory::Memcpy(InVtxBuf, InDrawList->VtxBuffer.Data, InDrawList->VtxBuffer.Size * sizeof(ImDrawVert));
	FMemory::Memcpy(InIdxBuf, InDrawList->IdxBuffer.Data, InDrawList->IdxBuffer.Size * sizeof(ImDrawIdx));

	// copy draw commands
	for (int32 CmdIdx = 0; CmdIdx < InDrawList->CmdBuffer.Size; ++CmdIdx)
	{
		ImDrawCmd& DrawCmd = InDrawList->CmdBuffer[CmdIdx];

		AllDrawElements.AddUninitialized();
		auto& Element = AllDrawElements.Top();
		Element.TextureID = DrawCmd.TextureId;
		Element.VtxOffset = DrawCmd.VtxOffset + BaseVtx;
		Element.IdxOffset = DrawCmd.IdxOffset + BaseIdx;
		Element.NumIndices = DrawCmd.ElemCount;
		FSlateRect Rect(*(FSlateRect*)&DrawCmd.ClipRect);
		Rect.Left += VertexOffset.X;
		Rect.Right += VertexOffset.X;
		Rect.Top += VertexOffset.Y;
		Rect.Bottom += VertexOffset.Y;
		Element.ScissorRect = ClippingRect.IntersectionWith(Rect);
	}
}

FRHITexture2D* FImguiDrawer::_GetTextureFromID(ImTextureID InID)
{
	if (!InID) return nullptr;
	auto ImResource = UImguiResourceManager::Get().FindResource(InID);
	if (!ImResource || !ImResource->Source || !ImResource->Source->GetResource()) return nullptr;
	return ImResource->Source->GetResource()->TextureRHI->GetTexture2D();
}

TSharedPtr<FImguiDrawer, ESPMode::ThreadSafe> FImguiDrawer::AllocDrawer()
{
	// find free drawer
	for (TSharedPtr<FImguiDrawer, ESPMode::ThreadSafe> Drawer : GlobalPool)
	{
		if (Drawer->bIsFree)
		{
			Drawer->bIsFree = false;
			return Drawer;
		}
	}

	// create new
	GlobalPool.Emplace(MakeShared<FImguiDrawer, ESPMode::ThreadSafe>());
	return GlobalPool.Top();
}

void FImguiDrawer::DrawRenderThread(FRHICommandListImmediate& RHICmdList, const void* RenderTarget)
{
	check(IsInRenderingThread());

	// early out
	if (AllDrawElements.Num() == 0)
	{
		bIsFree = true;
		return;
	}

	// copy data
	auto VtxBufSize = NumVertices * sizeof(ImDrawVert);
	auto IdxBufSize = NumTriangles * 3 * sizeof(ImDrawIdx);
	ImDrawVert* RHIVtxBuf = (ImDrawVert*)FRHICommandListExecutor::GetImmediateCommandList().LockBuffer(VertexBufferRHI, 0, VtxBufSize, EResourceLockMode::RLM_WriteOnly);
	ImDrawIdx*  RHIIdxBuf = (ImDrawIdx*)FRHICommandListExecutor::GetImmediateCommandList().LockBuffer(IndexBufferRHI, 0, IdxBufSize, EResourceLockMode::RLM_WriteOnly);

	FMemory::Memcpy(RHIVtxBuf, VtxBuf, VtxBufSize);
	FMemory::Memcpy(RHIIdxBuf, IdxBuf, IdxBufSize);
	
	FRHICommandListExecutor::GetImmediateCommandList().UnlockBuffer(VertexBufferRHI);
	FRHICommandListExecutor::GetImmediateCommandList().UnlockBuffer(IndexBufferRHI);
	
	// get render target 
	FTexture2DRHIRef* RT = (FTexture2DRHIRef*)RenderTarget;
	
	// begin pass
	FRHIRenderPassInfo PassInfo(*RT, ERenderTargetActions::Load_Store);
	RHICmdList.BeginRenderPass(PassInfo, TEXT("DrawImguiMesh"));
	
	// get shader
	TShaderMapRef<FImguiShaderVs> Vs(GetGlobalShaderMap(ERHIFeatureLevel::SM6));
	TShaderMapRef<FImguiShaderPs> Ps(GetGlobalShaderMap(ERHIFeatureLevel::SM6));

	// setup PSO
	FGraphicsPipelineStateInitializer PSOInitializer;
	RHICmdList.ApplyCachedRenderTargets(PSOInitializer);
	PSOInitializer.BoundShaderState.VertexDeclarationRHI = _GetMeshDeclaration();
	PSOInitializer.BoundShaderState.VertexShaderRHI = Vs.GetVertexShader();
	PSOInitializer.BoundShaderState.PixelShaderRHI = Ps.GetPixelShader();
	PSOInitializer.PrimitiveType = PT_TriangleList;
	PSOInitializer.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
	PSOInitializer.RasterizerState = TStaticRasterizerState<>::GetRHI();
	PSOInitializer.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	// init pso 
	SetGraphicsPipelineState(RHICmdList, PSOInitializer);

	// init texture 
	FRHITexture2D* CurTexture = _GetTextureFromID(AllDrawElements[0].TextureID);
	Ps->SetHasTexture(RHICmdList, Ps.GetPixelShader(), CurTexture != nullptr);
	if (CurTexture)
	{
		Ps->SetParameters(RHICmdList, Ps.GetPixelShader(), CurTexture);
	}
	
	// setup transform
	FMatrix RenderMatrix(FMatrix::Identity);
	RenderMatrix.M[0][0] = Scale;
	RenderMatrix.M[1][1] = Scale;
	RenderMatrix.M[3][0] = VertexOffset.X;
	RenderMatrix.M[3][1] = VertexOffset.Y;
	RenderMatrix *= OrthoMatrix;
	Vs->SetParameters(RHICmdList, RenderMatrix);

	// setup vertex buffer 
	RHICmdList.SetStreamSource(0, VertexBufferRHI, 0);

	// draw items 
	for (DrawElement& Element : AllDrawElements)
	{
		// early out
		if (!Element.NumIndices) continue;
		
		// get new texture
		FRHITexture2D* NewTexture = _GetTextureFromID(Element.TextureID);

		// change texture 
		if (NewTexture != CurTexture)
		{
			// setup texture state
			Ps->SetHasTexture(RHICmdList, Ps.GetPixelShader(), NewTexture != nullptr);
			
			// change texture
			if (NewTexture) Ps->SetParameters(RHICmdList, Ps.GetPixelShader(), NewTexture);

			// update current texture
			CurTexture = NewTexture;
		}
		
		// setup scissor rect 
		RHICmdList.SetScissorRect(
			true,
			Element.ScissorRect.Left,
			Element.ScissorRect.Top,
			Element.ScissorRect.Right,
			Element.ScissorRect.Bottom);

		// render mesh 
		RHICmdList.DrawIndexedPrimitive(
            IndexBufferRHI,
            Element.VtxOffset,
            0,
            NumVertices,
            Element.IdxOffset,
            Element.NumIndices / 3,
            1);
	}

	// end pass
	RHICmdList.EndRenderPass();

	// reset draw elements
	AllDrawElements.Reset();
	
	// mark free 
	bIsFree = true;
}
