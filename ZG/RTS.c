#include <malloc.h>
#include <stdlib.h>
#include <math.h>

#include "RTS.h"
#include "Math.h"

#define ZG_RTS_MAP_NODE_SIZE 16
#define ZG_RTS_BUFFER_SIZE 1024
#define ZG_RTS_OUTPUT_SIZE 1024
#define ZG_RTS_INFO_SIZE 1024

static ZGUINT8 sg_auBuffer[ZG_RTS_BUFFER_SIZE];
static ZGUINT8 sg_auOutput[ZG_RTS_OUTPUT_SIZE];
static ZGRTSINFO sg_aInfos[ZG_RTS_INFO_SIZE];
static ZGUINT sg_uOffset;
static ZGUINT sg_uCount;
static ZGUINT sg_uLevelSize;
static const ZGTILEMAP* sg_pTileMap;

ZGUINT __ZGRTSPredicate(const void* x, const void* y)
{
	if (x == ZG_NULL || y == ZG_NULL)
		return 0;

	ZGUINT uFromIndex = ((const ZGTILEMAPNODE*)x)->uIndex, uToIndex = ((const ZGTILEMAPNODE*)y)->uIndex;
	ZGFLOAT fDistance = (ZGRTSGetDistanceFromMap(sg_pTileMap, uFromIndex) + ZGRTSGetDistanceFromMap(sg_pTileMap, uToIndex)) * 0.5f;
	if (sg_pTileMap != ZG_NULL)
	{
		ZGUINT uFromLevel = uFromIndex / sg_uLevelSize,
			uFromMapIndex = uFromIndex - uFromLevel * sg_uLevelSize,
			uFromMapIndexX = uFromMapIndex % sg_pTileMap->Instance.uPitch,
			uFromMapIndexY = uFromMapIndex / sg_pTileMap->Instance.uPitch,
			uToLevel = uToIndex / sg_uLevelSize,
			uToMapIndex = uToIndex - uToLevel * sg_uLevelSize, 
			uToMapIndexX = uToMapIndex % sg_pTileMap->Instance.uPitch,
			uToMapIndexY = uToMapIndex / sg_pTileMap->Instance.uPitch,
			uDistanceX = ZG_ABS(uFromMapIndexX, uToMapIndexX),
			uDistanceY = ZG_ABS(uFromMapIndexY, uToMapIndexY), 
			uDistanceZ = ZG_ABS(uFromLevel, uToLevel);

		fDistance += sqrtf((ZGFLOAT)(uDistanceX * uDistanceX + uDistanceY * uDistanceY + uDistanceZ * uDistanceZ));
	}

	return (ZGUINT)roundf(fDistance);
}

ZGBOOLEAN __ZGRTSCheck(const void* pTileActionData, const void* pSourceTileNodeData, const void* pDestinationTileNodeData)
{
	if (pTileActionData == ZG_NULL || pDestinationTileNodeData == ZG_NULL)
		return ZG_FALSE;

	return ZG_TEST_FLAG(((const ZGRTSACTION*)pTileActionData)->uLabel, ((const ZGRTSNODE*)pDestinationTileNodeData)->uLabel);
}

ZGBOOLEAN __ZGRTSTestAction(const void* pTileNodeData, const LPZGTILENODE* ppTileNodes, ZGUINT uNodeCount)
{
	LPZGTILENODE pTileNode;
	for (ZGUINT i = 0; i < uNodeCount; ++i)
	{
		pTileNode = ppTileNodes[i];
		if (pTileNode != ZG_NULL && pTileNode->pData != ZG_NULL && pTileNode->pData != pTileNodeData)
			return ZG_FALSE;
	}

	return ZG_TRUE;
}

void __ZGRTSDelay(void* pTileNodeData, ZGUINT uElapsedTime, ZGUINT uTime)
{
	if (pTileNodeData == ZG_NULL)
		return;

	if (sg_uCount < ZG_RTS_INFO_SIZE)
	{
		LPZGRTSINFO pInfo = &sg_aInfos[sg_uCount++];
		pInfo->eType = ZG_RTS_INFO_TYPE_DELAY;
		pInfo->uElapsedTime = uElapsedTime;
		pInfo->uTime = uTime;
		pInfo->pTileManagerObject = ((LPZGRTSNODE)pTileNodeData)->pTileManagerObject;
	}
}

ZGBOOLEAN __ZGRTSHand(ZGUINT uTime, void* pUserData)
{
	if (pUserData == ZG_NULL)
		return ZG_FALSE;

	LPZGRTSHANDLER pHandler = (LPZGRTSHANDLER)pUserData;
	if (pHandler->uTime > uTime)
	{
		pHandler->uTime -= uTime;
		pHandler->uBreakTime = pHandler->uBreakTime > uTime ? pHandler->uBreakTime - uTime : 0;

		return ZG_TRUE;
	}

	if (pHandler->pfnInstance != ZG_NULL)
		pHandler->pfnInstance(pHandler->pTileObjectAction, pHandler->pSource, pHandler->pDestination, pHandler->uIndex, pHandler->uTime);

	return ZG_FALSE;
}

ZGBOOLEAN __ZGRTSCheckBreak(void* pUserData)
{
	return pUserData != ZG_NULL && ((LPZGRTSHANDLER)pUserData)->uBreakTime > 0;
}

ZGUINT __ZGRTSMove(
	ZGUINT uElapsedTime, 
	void* pTileObjectActionData,
	void* pTileNodeData, 
	LPZGTILEMAP pTileMap, 
	ZGUINT uFromIndex, 
	ZGUINT uToIndex)
{
	if (pTileNodeData == ZG_NULL || pTileMap == ZG_NULL)
		return 0;

	ZGUINT uTime = ((LPZGRTSNODE)pTileNodeData)->auAttributes[ZG_RTS_OBJECT_ATTRIBUTE_MOVE_TIME],
		uLevelSize = pTileMap->Instance.Instance.uCount / pTileMap->uLevel, 
		uFromLevel = uFromIndex / uLevelSize,
		uFromMapIndex = uFromIndex - uFromLevel * uLevelSize, 
		uFromMapIndexX = uFromMapIndex % pTileMap->Instance.uPitch,
		uFromMapIndexY = uFromMapIndex / pTileMap->Instance.uPitch,
		uToLevel = uToIndex / uLevelSize,
		uToMapIndex = uToIndex - uToLevel * uLevelSize,
		uToMapIndexX = uToMapIndex % pTileMap->Instance.uPitch,
		uToMapIndexY = uToMapIndex / pTileMap->Instance.uPitch,
		uDistanceX = ZG_ABS(uFromMapIndexX, uToMapIndexX), 
		uDistanceY = ZG_ABS(uFromMapIndexY, uToMapIndexY), 
		uDistanceZ = ZG_ABS(uFromLevel, uToLevel);
	uTime = (ZGUINT)roundf(
		uTime * 
		((ZGRTSGetDistanceFromMap(pTileMap, uFromIndex) + ZGRTSGetDistanceFromMap(pTileMap, uToIndex)) * 0.5f + 
			sqrtf((ZGFLOAT)(uDistanceX * uDistanceX + uDistanceY * uDistanceY + uDistanceZ * uDistanceZ))));
	if (sg_uCount < ZG_RTS_INFO_SIZE)
	{
		LPZGRTSINFO pInfo = &sg_aInfos[sg_uCount++];
		pInfo->eType = ZG_RTS_INFO_TYPE_MOVE;
		pInfo->uElapsedTime = uElapsedTime;
		pInfo->uTime = uTime;
		pInfo->pTileManagerObject = ((LPZGRTSNODE)pTileNodeData)->pTileManagerObject;
		LPZGRTSINFOMOVE pInfoMove = (LPZGRTSINFOMOVE)(sg_auOutput + sg_uOffset);

		sg_uOffset += sizeof(ZGRTSINFOMOVE);
		if (sg_uOffset > ZG_RTS_OUTPUT_SIZE)
			pInfo->pMove = ZG_NULL;
		else
		{
			pInfoMove->uFromIndex = uFromIndex;
			pInfoMove->uToIndex = uToIndex;

			pInfo->pMove = pInfoMove;
		}
	}

	return uTime;
}


ZGBOOLEAN __ZGRTSActiveAnalyzate(const void* pTileActionData, const void* pSourceTileNodeData, const void* pDestinationTileNodeData)
{
	if (pTileActionData == ZG_NULL)
		return ZG_FALSE;

	const ZGRTSACTION* pAction = (const ZGRTSACTION*)pTileActionData;
	if (ZG_TEST_BIT(pAction->uFlag, ZG_RTS_ACTION_TYPE_SELF))
		return pSourceTileNodeData == pDestinationTileNodeData;

	if (pDestinationTileNodeData == ZG_NULL)
		return ZG_FALSE;

	const ZGRTSNODE* pDestination = (const ZGRTSNODE*)pDestinationTileNodeData;
	if (pAction->pInstance != ZG_NULL && !ZG_TEST_FLAG(((LPZGRTSACTIONACTIVE)pAction->pInstance)->uLabel, pDestination->uLabel))
		return ZG_FALSE;

	if (pSourceTileNodeData == ZG_NULL)
		return ZG_FALSE;

	const ZGRTSNODE* pSource = (const ZGRTSNODE*)pSourceTileNodeData;
	if (ZG_TEST_BIT(pAction->uFlag, ZG_RTS_ACTION_TYPE_ALLY))
		return pSource->uCamp == pDestination->uCamp;

	if (ZG_TEST_BIT(pAction->uFlag, ZG_RTS_ACTION_TYPE_ENEMY))
		return pSource->uCamp != pDestination->uCamp;

	return ZG_FALSE;
}

ZGBOOLEAN __ZGRTSActiveHand(
	LPZGTILEOBJECTACTION pTileObjectAction,
	LPZGTILEMANAGEROBJECT pSource,
	LPZGTILEMANAGEROBJECT pDestination, 
	ZGUINT uIndex,
	ZGUINT uElapsedTime)
{
	if (pTileObjectAction == ZG_NULL || pSource == ZG_NULL)
		return ZG_FALSE;

	if (pSource->Instance.Instance.pTileMap == ZG_NULL)
		return ZG_FALSE;

	if (pTileObjectAction->pData == ZG_NULL)
		return ZG_FALSE;

	LPZGRTSACTIONACTIVE pActionActive = (LPZGRTSACTIONACTIVE)pTileObjectAction->pData;
	if (pActionActive->Instance.pInstance == ZG_NULL)
		return ZG_FALSE;

	if (ZG_TEST_BIT(pActionActive->Data.uFlag, ZG_RTS_ACTION_TYPE_TARGET))
		uIndex = pDestination->Instance.Instance.uIndex;

	if (uIndex < 0 || uIndex >= pSource->Instance.Instance.pTileMap->Instance.Instance.uCount)
		return ZG_FALSE;

	ZGMAP Map = pSource->Instance.Instance.pTileMap->Instance;
	Map.Instance.uCount /= pSource->Instance.Instance.pTileMap->uLevel;
	ZGUINT uLevel = uIndex / Map.Instance.uCount,
		uOffset = uLevel * Map.Instance.uCount;

	ZGUINT uLevelCount = pActionActive->Instance.pInstance->uLevelCount;
	PZGUINT puLevelIndices;
	if (uLevelCount > 0)
		puLevelIndices = pActionActive->Instance.pInstance->puLevelIndices;
	else
	{
		puLevelIndices = &uLevel;

		uLevelCount = 1;
	}

	ZGBOOLEAN bResult = ZG_FALSE;
	ZGUINT uMapIndex = uIndex - uOffset,
		uTemp = ZG_RTS_BUFFER_SIZE * sizeof(ZGUINT8) / sizeof(ZGUINT), 
		uCount, 
		uSize, 
		uLength, 
		uSource, 
		uDestination, 
		i, j, k;
	ZGINT nDamage;
	PZGUINT puIndices;
	LPZGTILENODE pTileNode, *ppTileNodes;
	LPZGRTSINFOHAND pInfoHand;
	LPZGRTSINFO pInfo;
	LPZGRTSNODE pSourceNode, pDestinationNode = (LPZGRTSNODE)pSource->Instance.Instance.pData;
	LPZGRTSINFOTARGET pInfoTargets = (LPZGRTSINFOTARGET)(sg_auOutput + sg_uOffset);
	for (i = 0; i < uLevelCount; ++i)
	{
		uOffset = puLevelIndices[i] * Map.Instance.uCount;
		Map.Instance.uOffset = pSource->Instance.Instance.pTileMap->Instance.Instance.uOffset + uOffset;
		uCount = uTemp;
		puIndices = (PZGUINT)sg_auBuffer;
		if (ZGMapTest(
			&Map,
			&pActionActive->Instance.pInstance->Instance.Instance,
			uMapIndex,
			pActionActive->Instance.pInstance->Instance.uOffset,
			&uCount,
			puIndices))
		{
			uSize = uCount * sizeof(ZGUINT) + sizeof(LPZGTILENODE);
			uLength = 0;
			ppTileNodes = (LPZGTILENODE*)(puIndices + uCount);
			for (j = 0; j < uCount; ++j)
			{
				pTileNode = ((LPZGTILENODEMAPNODE)ZGTileMapGetData(pSource->Instance.Instance.pTileMap, puIndices[j] + uOffset))->pNode;
				if (pTileNode == ZG_NULL)
					continue;

				if (pActionActive->pfnChecker != ZG_NULL &&
					!pActionActive->pfnChecker(pActionActive->Instance.pData, pSource->Instance.Instance.pData, pTileNode->pData))
					continue;

				for (k = 0; k < uLength; ++k)
				{
					if (ppTileNodes[k] == pTileNode)
						break;
				}

				if (k < uLength)
					continue;

				if (uSize > ZG_RTS_BUFFER_SIZE)
					++uLength;
				else
				{
					ppTileNodes[uLength++] = pTileNode;

					uSize += sizeof(LPZGTILENODE);
				}
			}

			if (uLength > 0)
			{
				//__ZGRTSDo(pTileObjectAction->pData, pTileManagerObject->Instance.Instance.pData, pTileMap, uIndex, uLength, ppTileNodes);
				if (sg_uCount < ZG_RTS_INFO_SIZE)
				{
					pInfo = &sg_aInfos[sg_uCount++];
					pInfo->eType = ZG_RTS_INFO_TYPE_HAND;
					pInfo->uElapsedTime = uElapsedTime;
					pInfo->uTime = 0;
					pInfo->pTileManagerObject = pSource;

					pInfoHand = (LPZGRTSINFOHAND)(sg_auOutput + sg_uOffset);

					sg_uOffset += sizeof(ZGRTSINFOHAND);
					if (sg_uOffset > ZG_RTS_OUTPUT_SIZE)
						pInfoHand = ZG_NULL;
					else
					{
						pInfoHand->uIndex = uIndex;
						pInfoHand->pTileObjectAction = pTileObjectAction;
						pInfoHand->uTargetCount = 0;
						pInfoHand->pTargets = ZG_NULL;
					}

					pInfo->pHand = pInfoHand;
				}
				else
					pInfoHand = ZG_NULL;

				if (pSource->Instance.Instance.pData != ZG_NULL)
				{
					for (j = 0; j < uCount; ++j)
					{
						pTileNode = ppTileNodes[j];
						if (pTileNode == ZG_NULL || pTileNode->pData == ZG_NULL)
							continue;

						pSourceNode = (LPZGRTSNODE)pTileNode->pData;

						nDamage = 0;
						for (k = 0; k < ZG_RTS_ELEMENT_COUNT; ++k)
						{
							uSource = pSourceNode->auAttributes[ZG_RTS_OBJECT_ATTRIBUTE_DEFENSE + k];
							uDestination = pDestinationNode->auAttributes[ZG_RTS_OBJECT_ATTRIBUTE_ATTACK + k];
							nDamage -= uSource < uDestination ? uDestination - uSource : 0;
						}

						pSourceNode->auAttributes[ZG_RTS_OBJECT_ATTRIBUTE_HP] += nDamage;
						/*if (lHp <= 0)
						ZGTileNodeUnset(pTileNode, pTileMap);*/

						if (pInfoHand != ZG_NULL)
						{
							sg_uOffset += sizeof(ZGRTSINFOTARGET);
							if (sg_uOffset <= ZG_RTS_BUFFER_SIZE)
							{
								pInfoTargets->pTileManagerObject = pSourceNode->pTileManagerObject;
								pInfoTargets->nDamage = nDamage;

								if (pInfoHand->pTargets == ZG_NULL)
									pInfoHand->pTargets = pInfoTargets;

								++pInfoTargets;

								++pInfoHand->uTargetCount;
							}
						}
					}
				}

				bResult = ZG_TRUE;
			}
		}
	}

	return bResult;
}

ZGUINT __ZGRTSActiveSet(
	ZGUINT uElapsedTime, 
	void* pTileObjectActionData,
	void* pTileNodeData, 
	LPZGTILEMAP pTileMap, 
	ZGUINT uIndex, 
	void** ppUserData)
{
	if (pTileObjectActionData == ZG_NULL || pTileNodeData == ZG_NULL)
		return 0;

	LPZGTILEOBJECTACTION pTileObjectAction = ((LPZGRTSACTIONACTIVE)pTileObjectActionData)->pTileObjectAction;
	LPZGRTSNODE pNode = (LPZGRTSNODE)pTileNodeData;
	LPZGTILEACTIONMAPNODE pTileActionMapNode = (LPZGTILEACTIONMAPNODE)((LPZGTILENODEMAPNODE)ZGTileMapGetData(pTileMap, uIndex))->pData;
	if (ppUserData != ZG_NULL && *ppUserData != ZG_NULL)
	{
		LPZGRTSHANDLER pHandler = (LPZGRTSHANDLER)(*ppUserData);
		pHandler->uTime = uElapsedTime + pNode->auAttributes[ZG_RTS_OBJECT_ATTRIBUTE_HAND_TIME];
		pHandler->uBreakTime = uElapsedTime + pNode->auAttributes[ZG_RTS_OBJECT_ATTRIBUTE_BREAK_TIME];
		pHandler->uIndex = pTileActionMapNode->uMaxIndex;
		LPZGTILENODE pTileNode = pTileActionMapNode->uMaxCount > 0 ? pTileActionMapNode->ppNodes[0] : ZG_NULL;
		void* pData = pTileNode == ZG_NULL ? ZG_NULL : pTileNode->pData;
		pHandler->pDestination = pData == ZG_NULL ? ZG_NULL : ((LPZGRTSNODE)pData)->pTileManagerObject;
		pHandler->pSource = pNode->pTileManagerObject;
		pHandler->pTileObjectAction = pTileObjectAction;
		pHandler->pfnInstance = __ZGRTSActiveHand;
		*ppUserData = pHandler;
	}

	ZGUINT uTime = pNode->auAttributes[ZG_RTS_OBJECT_ATTRIBUTE_SET_TIME];
	if (sg_uCount < ZG_RTS_INFO_SIZE)
	{
		LPZGRTSINFO pInfo = &sg_aInfos[sg_uCount++];
		pInfo->eType = ZG_RTS_INFO_TYPE_SET;
		pInfo->uElapsedTime = uElapsedTime;
		pInfo->uTime = uTime;
		pInfo->pTileManagerObject = pNode->pTileManagerObject;
		LPZGRTSINFOSET pInfoSet = (LPZGRTSINFOSET)(sg_auOutput + sg_uOffset);

		sg_uOffset += sizeof(ZGRTSINFOSET);
		if (sg_uOffset > ZG_RTS_OUTPUT_SIZE)
			pInfoSet = ZG_NULL;
		else
		{
			pInfoSet->uIndex = pTileActionMapNode->uMaxIndex;
			pInfoSet->pTileObjectAction = pTileObjectAction;
		}

		pInfo->pSet = pInfoSet;
	}

	return uTime;
}

ZGUINT __ZGRTSActiveCheck(
	void* pTileObjectActionData,
	LPZGTILENODE pTileNode)
{
	if (pTileObjectActionData == ZG_NULL)
		return 0;

	sg_pTileMap = pTileNode == ZG_NULL ? ZG_NULL : pTileNode->pTileMap;
	sg_uLevelSize = sg_pTileMap == ZG_NULL ? 0 : sg_pTileMap->Instance.Instance.uCount / sg_pTileMap->uLevel;

	return ZGTileActionSearch(
		&((LPZGRTSACTIONACTIVE)pTileObjectActionData)->Instance,
		pTileNode,
		ZG_RTS_BUFFER_SIZE,
		sg_auBuffer,
		__ZGRTSPredicate, 
		ZG_NULL, 
		__ZGRTSTestAction, 
		ZG_NODE_SEARCH_TYPE_ONCE);
}

ZGUINT __ZGRTSNormalCheck(
	void* pTileObjectActionData,
	LPZGTILENODE pTileNode)
{
	if (pTileObjectActionData == ZG_NULL || pTileNode == ZG_NULL)
		return ZG_NULL;

	if (pTileNode->pTileMap == ZG_NULL || 
		pTileNode->pTileMap->pNodes == ZG_NULL || 
		pTileNode->uIndex >= pTileNode->pTileMap->Instance.Instance.uCount)
		return ZG_NULL;

	LPZGRTSACTIONNORMAL pActionNormal = (LPZGRTSACTIONNORMAL)pTileObjectActionData;
	ZGUINT uLevelSize = pTileNode->pTileMap->Instance.Instance.uCount / pTileNode->pTileMap->uLevel,
		uLevel = pTileNode->uIndex / uLevelSize, 
		uOffset = uLevel * uLevelSize, 
		uMapIndex = pTileNode->uIndex - uOffset,
		uSourceMapIndexX = uMapIndex % pTileNode->pTileMap->Instance.uPitch,
		uSourceMapIndexY = uMapIndex / pTileNode->pTileMap->Instance.uPitch,
		uDestinationMapIndexX, 
		uDestinationMapIndexY;
	if (pActionNormal->uMapIndex == uMapIndex || pActionNormal->uMapIndex >= uLevelSize)
	{
		ZGUINT uWidth = pTileNode->pTileMap->Instance.uPitch, uHeight = uLevelSize / uWidth;
		--uWidth;
		--uHeight;

		uDestinationMapIndexX = uSourceMapIndexX + (ZGUINT)(rand() * 2.0f / RAND_MAX * pActionNormal->uRange);
		uDestinationMapIndexX = uDestinationMapIndexX > pActionNormal->uRange ? uDestinationMapIndexX - pActionNormal->uRange : 0;
		uDestinationMapIndexX = uDestinationMapIndexX > uWidth ? uWidth : uDestinationMapIndexX;
		uDestinationMapIndexY = uSourceMapIndexY + (ZGUINT)(rand() * 2.0f / RAND_MAX * pActionNormal->uRange);
		uDestinationMapIndexY = uDestinationMapIndexY > pActionNormal->uRange ? uDestinationMapIndexY - pActionNormal->uRange : 0;
		uDestinationMapIndexY = uDestinationMapIndexY > uHeight ? uHeight : uDestinationMapIndexY;

		pActionNormal->uMapIndex = uSourceMapIndexX + uDestinationMapIndexY * pTileNode->pTileMap->Instance.uPitch;
	}
	else
	{
		uDestinationMapIndexX = pActionNormal->uMapIndex % pTileNode->pTileMap->Instance.uPitch;
		uDestinationMapIndexY = pActionNormal->uMapIndex / pTileNode->pTileMap->Instance.uPitch;
	}

	ZGMAP Map = pTileNode->pTileMap->Instance;
	Map.Instance.uOffset += uOffset;
	Map.Instance.uCount = uLevelSize;

	const ZGUINT uCOUNT = ZG_RTS_BUFFER_SIZE * sizeof(ZGUINT8) / sizeof(ZGUINT);
	ZGUINT uDistanceX = ZG_ABS(uSourceMapIndexX, uDestinationMapIndexX),
		uDistanceY = ZG_ABS(uSourceMapIndexY, uDestinationMapIndexY),
		uDepth = 1, 
		uIndex, 
		uCount, 
		uSize, 
		uLength, 
		i, j;
	PZGINT puIndices = (PZGUINT)sg_auBuffer;
	LPZGNODE pNode = pTileNode->pTileMap->pNodes + pTileNode->uIndex;
	LPZGTILENODE pTemp;
	LPZGTILENODE* ppTileNodes;
	pNode->uValue = 0;
	pNode->uDistance = 0;
	pNode->uDepth = 0;
	pNode->pPrevious = ZG_NULL;
	while (uDistanceX > 0 || uDistanceY > 0)
	{
		if (uDistanceX > 0)
		{
			uSourceMapIndexX = uSourceMapIndexX > uDestinationMapIndexX ? uSourceMapIndexX - 1 : uSourceMapIndexX + 1;

			--uDistanceX;
		}
		
		if (uDistanceY > 0)
		{
			uSourceMapIndexY = uSourceMapIndexY > uDestinationMapIndexY ? uSourceMapIndexY - 1 : uSourceMapIndexY + 1;

			--uDistanceY;
		}

		uIndex = uSourceMapIndexX + uSourceMapIndexY * Map.uPitch;
		uCount = uCOUNT;
		if (pTileNode->pInstance != ZG_NULL && 
			ZGMapTest(
				&Map,
				&pTileNode->pInstance->Instance.Instance, 
				uIndex,
				pTileNode->pInstance->Instance.uOffset,
				&uCount, 
				puIndices))
		{
			uSize = uCount * sizeof(ZGUINT) + sizeof(LPZGTILENODE);
			if (uSize <= ZG_RTS_BUFFER_SIZE)
			{
				uLength = 0;
				ppTileNodes = (LPZGTILENODE*)(puIndices + uCount);
				for (i = 0; i < uCount; ++i)
				{
					pTemp = ((LPZGTILENODEMAPNODE)ZGTileMapGetData(pTileNode->pTileMap, puIndices[i] + uOffset))->pNode;
					if (pTemp == ZG_NULL)
						break;

					if (pTemp == pTileNode)
						continue;

					for (j = 0; j < uLength; ++j)
					{
						if (ppTileNodes[j] == pTemp)
							break;
					}

					if (j < uLength)
						continue;

					ppTileNodes[uLength++] = pTemp;

					uSize += sizeof(LPZGTILENODE);
					if (uSize > ZG_RTS_BUFFER_SIZE)
					{
						i = uCount;

						break;
					}
				}

				if (i < uCount || (uLength > 0 && !__ZGRTSTestAction(pTileNode->pData, ppTileNodes, uLength)))
					break;
			}
		}

		pNode->pNext = pTileNode->pTileMap->pNodes + uIndex + uOffset;
		pNode->pNext->uDepth = pNode->uDepth + 1;
		pNode->pNext->uDistance = __ZGRTSPredicate(pNode->pData, pNode->pNext->pData);
		pNode->pNext->uValue = pNode->uValue + pNode->pNext->uDistance;
		pNode->pNext->uEvaluation = 0;
		pNode->pNext->pPrevious = pNode;
		pNode = pNode->pNext;

		++uDepth;
	}

	pNode->pNext = ZG_NULL;

	if(uDistanceX > 0 || uDistanceY > 0)
		pActionNormal->uMapIndex = ~0;

	return uDepth;
}

void ZGRTSDestroy(void* pData)
{
	free(pData);
}

LPZGTILEMANAGEROBJECT ZGRTSCreateObject(
	ZGUINT uWidth,
	ZGUINT uHeight,
	ZGUINT uOffset)
{
	ZGUINT uCount = uWidth * uHeight, uLength = (uCount + 7) >> 3;
	LPZGTILEMANAGEROBJECT pResult = (LPZGTILEMANAGEROBJECT)malloc(
		sizeof(ZGTILEMANAGEROBJECT) +
		sizeof(ZGTILENODEDATA) +
		sizeof(ZGUINT8) * uLength +
		sizeof(ZGRTSNODE));
	LPZGTILENODEDATA pTileNodeData = (LPZGTILENODEDATA)(pResult + 1);
	PZGUINT8 puFlags = (PZGUINT8)(pTileNodeData + 1);
	LPZGRTSNODE pNode = (LPZGRTSNODE)(puFlags + uLength);

	pResult->Instance.Instance.pInstance = pTileNodeData;
	pResult->Instance.Instance.pData = pNode;
	pResult->Instance.Instance.pTileMap = ZG_NULL;
	pResult->Instance.Instance.uIndex = ~0;

	pResult->Instance.nTime = 0;

	pResult->Instance.pActions = ZG_NULL;
	pResult->pHandler = ZG_NULL;
	pResult->pPrevious = ZG_NULL;
	pResult->pNext = ZG_NULL;

	pTileNodeData->Instance.Instance.Instance.puFlags = puFlags;
	pTileNodeData->Instance.Instance.Instance.uOffset = 0;
	pTileNodeData->Instance.Instance.Instance.uCount = uCount;
	pTileNodeData->Instance.Instance.uPitch = uWidth;
	pTileNodeData->Instance.uOffset = uOffset;

	pTileNodeData->uDistance = 0;
	pTileNodeData->uRange = 0;

	ZGUINT i;
	--uLength;

	for (i = 0; i < uLength; ++i)
		puFlags[i] = ~0;

	uCount &= 7;
	puFlags[uLength] = uCount > 0 ? (1 << uCount) - 1 : ~0;

	pNode->pTileManagerObject = pResult;
	pNode->uCamp = 0;
	pNode->uLabel = 0;

	for (ZGUINT i = 0; i < ZG_RTS_OBJECT_ATTRIBUTE_COUNT; ++i)
		pNode->auAttributes[i] = 0;

	return pResult;
}

LPZGTILEOBJECTACTION ZGRTSCreateActionNormal(ZGUINT uChildCount)
{
	LPZGTILEOBJECTACTION pResult = (LPZGTILEOBJECTACTION)malloc(
		sizeof(ZGTILEOBJECTACTION) +
		sizeof(ZGRTSACTIONNORMAL) +
		sizeof(LPZGTILEOBJECTACTION) * uChildCount);
	LPZGRTSACTIONNORMAL pActionNormal = (LPZGRTSACTIONNORMAL)(pResult + 1);
	pResult->pfnCheck = __ZGRTSNormalCheck;
	pResult->pfnMove = __ZGRTSMove;
	pResult->pfnSet = ZG_NULL;
	pResult->pData = pActionNormal;
	pResult->uChildCount = uChildCount;
	pResult->ppChildren = (LPZGTILEOBJECTACTION*)(pActionNormal + 1);

	pActionNormal->uMapIndex = ~0;
	pActionNormal->uRange = 5;

	for (ZGUINT i = 0; i < uChildCount; ++i)
		pResult->ppChildren[i] = ZG_NULL;

	return pResult;
}

LPZGTILEOBJECTACTION ZGRTSCreateActionActive(
	ZGUINT uDistance,
	ZGUINT uRange,
	ZGUINT uChildCount)
{
	ZGUINT uDistanceLength = (uDistance << 1) + 1, uRangeLength = (uRange << 1) + 1;
	uDistanceLength = (uDistanceLength * uDistanceLength + 7) >> 3;
	uRangeLength = (uRangeLength * uRangeLength + 7) >> 3;
	LPZGTILEOBJECTACTION pResult = (LPZGTILEOBJECTACTION)malloc(
		sizeof(ZGTILEOBJECTACTION) +
		sizeof(ZGTILEACTIONDATA) +
		sizeof(ZGRTSACTIONACTIVE) +
		sizeof(ZGUINT8) * uDistanceLength +
		sizeof(ZGUINT8) * uRangeLength +
		sizeof(LPZGTILEOBJECTACTION) * uChildCount);
	LPZGTILEACTIONDATA pTileActionData = (LPZGTILEACTIONDATA)(pResult + 1);
	LPZGRTSACTIONACTIVE pActionActive = (LPZGRTSACTIONACTIVE)(pTileActionData + 1);
	PZGUINT8 puDistanceFlags = (PZGUINT8)(pActionActive + 1), puRangeFlags = puDistanceFlags + uDistanceLength;
	pResult->pfnCheck = __ZGRTSActiveCheck;
	pResult->pfnMove = __ZGRTSMove;
	pResult->pfnSet = __ZGRTSActiveSet;
	pResult->pData = pActionActive;
	pResult->uChildCount = uChildCount;
	pResult->ppChildren = (LPZGTILEOBJECTACTION*)(puRangeFlags + uRangeLength);

	ZGTileRangeInitOblique(&pTileActionData->Distance, puDistanceFlags, uDistance);
	ZGTileRangeInitOblique(&pTileActionData->Instance, puRangeFlags, uRange);

	pTileActionData->puLevelIndices = ZG_NULL;
	pTileActionData->uLevelCount = 0;

	pActionActive->uLabel = 0;

	pActionActive->Instance.pInstance = pTileActionData;
	pActionActive->Instance.uEvaluation = 0;
	pActionActive->Instance.uMinEvaluation = 0;
	pActionActive->Instance.uMaxEvaluation = 0;
	pActionActive->Instance.uMaxDistance = 0;
	pActionActive->Instance.uMaxDepth = 0;
	pActionActive->Instance.pData = &pActionActive->Data;
	pActionActive->Instance.pfnAnalyzation = __ZGRTSActiveAnalyzate;

	pActionActive->Data.uFlag = 1 << ZG_RTS_ACTION_TYPE_ENEMY;
	pActionActive->Data.uLabel = 0;

	pActionActive->Data.pInstance = pActionActive;

	pActionActive->pTileObjectAction = pResult;

	pActionActive->pfnChecker = __ZGRTSCheck;

	for (ZGUINT i = 0; i < uChildCount; ++i)
		pResult->ppChildren[i] = ZG_NULL;

	return pResult;
}

LPZGTILEMAP ZGRTSCreateMap(ZGUINT uWidth, ZGUINT uHeight, ZGUINT uDepth, ZGBOOLEAN bIsOblique)
{
	ZGUINT uCount = uWidth * uHeight * uDepth, 
		uFlagLength = (uCount + 7) >> 3, 
		uChildCount = ZGTileChildCount(uWidth, uHeight, bIsOblique) * uDepth, 
		uNodeCount = uCount * ZG_RTS_MAP_NODE_SIZE;
	LPZGTILEMAP pResult = (LPZGTILEMAP)malloc(
		sizeof(ZGTILEMAP) +
		sizeof(ZGUINT8) * uFlagLength +
		sizeof(LPZGNODE) * uChildCount +
		sizeof(ZGNODE) * uCount +
		sizeof(LPZGTILENODE) * uNodeCount +
		sizeof(ZGTILEMAPNODE) * uCount +
		sizeof(ZGTILENODEMAPNODE) * uCount +
		sizeof(ZGTILEACTIONMAPNODE) * uCount + 
		sizeof(ZGRTSMAPNODE) * uCount);
	PZGUINT8 puFlags = (PZGUINT8)(pResult + 1);
	LPZGNODE* ppNodes = (LPZGNODE*)(puFlags + uFlagLength), pNodes = (LPZGNODE)(ppNodes + uChildCount);
	LPZGTILENODE* ppTileNodes = (LPZGTILENODE*)(pNodes + uCount);
	LPZGTILEMAPNODE pTileMapNodes = (LPZGTILEMAPNODE)(ppTileNodes + uNodeCount);
	LPZGTILENODEMAPNODE pTileNodeMapNodes = (LPZGTILENODEMAPNODE)(pTileMapNodes + uCount);
	LPZGTILEACTIONMAPNODE pTileActionMapNodes = (LPZGTILEACTIONMAPNODE)(pTileNodeMapNodes + uCount);
	LPZGRTSMAPNODE pMapNodes = (LPZGRTSMAPNODE)(pTileActionMapNodes + uCount);

	ZGTileMapEnable(pResult, puFlags, ppNodes, pNodes, pTileMapNodes, ZG_NULL, 0, uWidth, uHeight, uDepth, bIsOblique);

	ZGUINT i;
	for (i = 0; i < uFlagLength; ++i)
		puFlags[i] = 0;

	for (i = 0; i < uCount; ++i)
	{
		pMapNodes->uDistance = 1;

		pTileActionMapNodes->ppNodes = ppTileNodes + i * ZG_RTS_MAP_NODE_SIZE;
		pTileActionMapNodes->uCount = ZG_RTS_MAP_NODE_SIZE;
		pTileActionMapNodes->pData = pMapNodes++;

		pTileNodeMapNodes->pNode = ZG_NULL;
		pTileNodeMapNodes->pData = pTileActionMapNodes++;

		((LPZGTILEMAPNODE)pResult->pNodes[i].pData)->pData = pTileNodeMapNodes++;
	}

	return pResult;
}

LPZGTILEMANAGER ZGRTSCreateManager(ZGUINT uCapacity)
{
	LPZGTILEMANAGER pResult = (LPZGTILEMANAGER)malloc(
		sizeof(ZGTILEMANAGER) + 
		sizeof(ZGTILEMANAGERHANDLER) * uCapacity + 
		sizeof(ZGRTSHANDLER) * uCapacity);
	LPZGTILEMANAGERHANDLER pTileManagerHandlers = (LPZGTILEMANAGERHANDLER)(pResult + 1);
	LPZGRTSHANDLER pHandlers = (LPZGRTSHANDLER)(pTileManagerHandlers + uCapacity);
	pResult->pObjects = ZG_NULL;
	pResult->pQueue = ZG_NULL;
	
	ZGUINT i;
	LPZGTILEMANAGERHANDLER pTileManagerHandler = pTileManagerHandlers;
	for (i = 1; i < uCapacity; ++i)
	{
		pTileManagerHandler = pTileManagerHandlers + i;
		pTileManagerHandler->pUserData = pHandlers + i;
		pTileManagerHandler->pNext = pTileManagerHandlers + i - 1;
	}

	if (uCapacity > 0)
	{
		pTileManagerHandlers->pUserData = pHandlers;
		/*pTileManagerHandlers->pObject = ZG_NULL;
		pTileManagerHandlers->pBackward = ZG_NULL;
		pTileManagerHandlers->pForward = ZG_NULL;*/
		pTileManagerHandlers->pNext = ZG_NULL;

		pResult->pPool = pTileManagerHandler;
	}
	else
		pResult->pPool = ZG_NULL;

	return pResult;
}

void ZGRTSDestroyMap(LPZGTILEMAP pTileMap)
{
	ZGTileMapDisable(pTileMap);

	free(pTileMap);
}

ZGBOOLEAN ZGRTSGetMap(LPZGTILEMAP pTileMap, ZGUINT uIndex)
{
	return ZGTileMapGet(pTileMap, uIndex);
}

ZGBOOLEAN ZGRTSSetMap(LPZGTILEMAP pTileMap, ZGUINT uIndex, ZGBOOLEAN bValue)
{
	return ZGTileMapSet(pTileMap, uIndex, bValue);
}

ZGBOOLEAN ZGRTSSetObjectToMap(LPZGTILEMANAGEROBJECT pTileManagerObject, LPZGTILEMAP pTileMap, ZGUINT uIndex)
{
	if (pTileManagerObject == ZG_NULL || pTileMap == ZG_NULL)
		return ZG_FALSE;

	if (pTileManagerObject->Instance.Instance.uIndex < pTileMap->Instance.Instance.uCount)
		return ZG_FALSE;

	if (ZGMapTest(
		&pTileMap->Instance,
		&pTileManagerObject->Instance.Instance.pInstance->Instance.Instance,
		uIndex,
		pTileManagerObject->Instance.Instance.pInstance->Instance.uOffset,
		ZG_NULL,
		ZG_NULL))
		return ZG_FALSE;

	return ZGTileNodeSetTo(&pTileManagerObject->Instance.Instance, pTileMap, uIndex);
}

ZGBOOLEAN ZGRTSUnsetObjectFromMap(LPZGTILEMANAGEROBJECT pTileManagerObject)
{
	if (pTileManagerObject == ZG_NULL)
		return ZG_FALSE;

	return ZGTileNodeUnset(&pTileManagerObject->Instance.Instance);
}

ZGBOOLEAN ZGRTSAddObjectToManager(LPZGTILEMANAGEROBJECT pTileManagerObject, LPZGTILEMANAGER pTileManager)
{
	return ZGTileManagerAdd(pTileManager, pTileManagerObject);
}

ZGBOOLEAN ZGRTSRemoveObjectFromManager(LPZGTILEMANAGEROBJECT pTileManagerObject, LPZGTILEMANAGER pTileManager)
{
	return ZGTileManagerRemove(pTileManager, pTileManagerObject);
}

ZGBOOLEAN ZGRTSDo(
	LPZGTILEMANAGER pTileManager,
	LPZGTILEMANAGEROBJECT pTileManagerObject,
	ZGUINT uIndex,
	ZGUINT uTime, 
	PZGUINT puInfoCount,
	LPZGRTSINFO* ppInfos)
{
	if (pTileManagerObject == ZG_NULL)
		return ZG_NULL;

	if (pTileManagerObject->Instance.pActions == ZG_NULL)
		return ZG_NULL;

	sg_uOffset = 0;
	sg_uCount = 0;

	void* pData = ZGTileMapGetData(pTileManagerObject->Instance.Instance.pTileMap, uIndex);
	pData = pData == ZG_NULL ? ZG_NULL : ((LPZGTILENODEMAPNODE)pData)->pData;
	if (pData != ZG_NULL)
	{
		LPZGTILEACTIONMAPNODE pTileActionMapNode = (LPZGTILEACTIONMAPNODE)pData;
		pTileActionMapNode->uMaxCount = 0;
		pTileActionMapNode->uMaxIndex = uIndex;
	}

	ZGBOOLEAN bResult = ZGTileManagerSet(pTileManager, pTileManagerObject, pTileManagerObject->Instance.pActions, uIndex, uTime);

	if (puInfoCount != ZG_NULL)
		*puInfoCount = sg_uCount;

	if (ppInfos != ZG_NULL)
		*ppInfos = sg_aInfos;

	return bResult;
}

void ZGRTSBreak(LPZGTILEMANAGEROBJECT pTileManagerObject, ZGUINT uTime)
{
	ZGTileManagerBreak(pTileManagerObject, uTime, __ZGRTSCheckBreak);
}

LPZGRTSINFO ZGRTSRun(LPZGTILEMANAGER pTileManager, ZGUINT uTime, PZGUINT puInfoCount)
{
	sg_uOffset = 0;
	sg_uCount = 0;

	ZGTileManagerRun(
		pTileManager,
		uTime,
		__ZGRTSDelay, 
		__ZGRTSHand);

	if (puInfoCount != ZG_NULL)
		*puInfoCount = sg_uCount;

	return sg_aInfos;
}
