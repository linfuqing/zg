#pragma once

#include "TileNode.h"

#ifdef __cplusplus
extern "C" {
#endif
	typedef ZGUINT(*ZGTILEACTIONEVALUATION)(const ZGTILEMAP* pTileMap, ZGUINT uIndex);
	typedef ZGBOOLEAN (*ZGTILEACTIONTEST)(const void* pTileNodeData, const LPZGTILENODE* ppTileNodes, ZGUINT uNodeCount);
	typedef ZGBOOLEAN (*ZGTILEACTIONANALYZATION)(const void* pTileActionData, const void* pSourceTileNodeData, const void* pDestinationTileNodeData);

	typedef struct ZGTileActionMapNode
	{
		LPZGTILENODE* ppNodes;
		ZGUINT uCount;
		ZGUINT uMaxCount;
		ZGUINT uMaxIndex;

		void* pData;
	}ZGTILEACTIONMAPNODE, *LPZGTILEACTIONMAPNODE;

	typedef struct ZGTileActionData
	{
		ZGTILERANGE Instance;
		ZGTILERANGE Distance;

		PZGUINT puLevelIndices;
		ZGUINT uLevelCount;
	}ZGTILEACTIONDATA, *LPZGTILEACTIONDATA;

	typedef struct ZGTileAction
	{
		LPZGTILEACTIONDATA pInstance;

		ZGUINT uEvaluation;
		ZGUINT uMinEvaluation;
		ZGUINT uMaxEvaluation;
		ZGUINT uMaxDistance;
		ZGUINT uMaxDepth;

		void* pData;

		ZGTILEACTIONANALYZATION pfnAnalyzation;
	}ZGTILEACTION, *LPZGTILEACTION;

	ZGUINT ZGTileActionSearch(
		const ZGTILEACTION* pTileAction,
		LPZGTILENODE pTileNode,
		ZGUINT uBufferLength,
		PZGUINT8 puBuffer,
		ZGNODEPREDICATION pfnPredication,
		ZGTILEACTIONEVALUATION pfnTileActionEvaluation,
		ZGTILEACTIONTEST pfnTileActionTest, 
		ZGNODESEARCHTYPE eType);
#ifdef __cplusplus
}
#endif


