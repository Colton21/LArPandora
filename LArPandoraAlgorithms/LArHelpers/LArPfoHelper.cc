/**
 *  @file   LArContent/src/LArHelpers/LArPfoHelper.cc
 *
 *  @brief  Implementation of the pfo helper class.
 *
 *  $Log: $
 */

#include "LArPandoraAlgorithms/LArHelpers/LArPfoHelper.h"
#include "LArPandoraAlgorithms/LArHelpers/LArClusterHelper.h"

#include "LArPandoraAlgorithms/LArObjects/LArThreeDSlidingFitResult.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace pandora;

namespace lar_content
{

void LArPfoHelper::GetCaloHits(const PfoList &pfoList, const HitType &hitType, CaloHitList &caloHitList)
{
    for (PfoList::const_iterator pIter = pfoList.begin(), pIterEnd = pfoList.end(); pIter != pIterEnd; ++pIter)
    {
        LArPfoHelper::GetCaloHits(*pIter, hitType, caloHitList);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void LArPfoHelper::GetCaloHits(const ParticleFlowObject *const pPfo, const HitType &hitType, CaloHitList &caloHitList)
{
    ClusterList clusterList;
    LArPfoHelper::GetClusters(pPfo, hitType, clusterList);

    for (ClusterList::const_iterator cIter = clusterList.begin(), cIterEnd = clusterList.end(); cIter != cIterEnd; ++cIter)
    {
        (*cIter)->GetOrderedCaloHitList().GetCaloHitList(caloHitList);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void LArPfoHelper::GetClusters(const PfoList &pfoList, const HitType &hitType, ClusterList &clusterList)
{
    for (PfoList::const_iterator pIter = pfoList.begin(), pIterEnd = pfoList.end(); pIter != pIterEnd; ++pIter)
    {
        LArPfoHelper::GetClusters(*pIter, hitType, clusterList);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void LArPfoHelper::GetClusters(const ParticleFlowObject *const pPfo, const HitType &hitType, ClusterList &clusterList)
{
    const ClusterList &pfoClusterList = pPfo->GetClusterList();
    for (ClusterList::const_iterator cIter = pfoClusterList.begin(), cIterEnd = pfoClusterList.end(); cIter != cIterEnd; ++cIter)
    {
        Cluster *pPfoCluster = *cIter;

        if (hitType != LArClusterHelper::GetClusterHitType(pPfoCluster))
            continue;

        clusterList.insert(pPfoCluster);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void LArPfoHelper::GetAllConnectedPfos(const PfoList &inputPfoList, PfoList &outputPfoList)
{
    for (PfoList::const_iterator pIter = inputPfoList.begin(), pIterEnd = inputPfoList.end(); pIter != pIterEnd; ++pIter)
    {
        LArPfoHelper::GetAllConnectedPfos(*pIter, outputPfoList);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void LArPfoHelper::GetAllConnectedPfos(ParticleFlowObject *const pPfo, PfoList &outputPfoList)
{
    if (outputPfoList.count(pPfo))
        return;

    outputPfoList.insert(pPfo);
    LArPfoHelper::GetAllConnectedPfos(pPfo->GetParentPfoList(), outputPfoList);
    LArPfoHelper::GetAllConnectedPfos(pPfo->GetDaughterPfoList(), outputPfoList);
}

//------------------------------------------------------------------------------------------------------------------------------------------

void LArPfoHelper::GetAllDownstreamPfos(const PfoList &inputPfoList, PfoList &outputPfoList)
{
    for (PfoList::const_iterator pIter = inputPfoList.begin(), pIterEnd = inputPfoList.end(); pIter != pIterEnd; ++pIter)
    {
        LArPfoHelper::GetAllDownstreamPfos(*pIter, outputPfoList);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void LArPfoHelper::GetAllDownstreamPfos(ParticleFlowObject *const pPfo, PfoList &outputPfoList)
{
    if (outputPfoList.count(pPfo))
        return;

    outputPfoList.insert(pPfo);
    LArPfoHelper::GetAllDownstreamPfos(pPfo->GetDaughterPfoList(), outputPfoList);
}

//------------------------------------------------------------------------------------------------------------------------------------------

float LArPfoHelper::GetTwoDLengthSquared(const ParticleFlowObject *const pPfo)
{
    float lengthSquared(0.f);

    const ClusterList &pfoClusterList = pPfo->GetClusterList();
    for (ClusterList::const_iterator cIter = pfoClusterList.begin(), cIterEnd = pfoClusterList.end(); cIter != cIterEnd; ++cIter)
    {
        const Cluster *pPfoCluster = *cIter;

        if (TPC_3D == LArClusterHelper::GetClusterHitType(pPfoCluster))
            continue;

        lengthSquared += LArClusterHelper::GetLengthSquared(pPfoCluster);
    }

    return lengthSquared;
}

//------------------------------------------------------------------------------------------------------------------------------------------

float LArPfoHelper::GetThreeDLengthSquared(const ParticleFlowObject *const pPfo)
{
    float lengthSquared(0.f);

    const ClusterList &pfoClusterList = pPfo->GetClusterList();
    for (ClusterList::const_iterator cIter = pfoClusterList.begin(), cIterEnd = pfoClusterList.end(); cIter != cIterEnd; ++cIter)
    {
        const Cluster *pPfoCluster = *cIter;

        if (TPC_3D != LArClusterHelper::GetClusterHitType(pPfoCluster))
            continue;

        lengthSquared += LArClusterHelper::GetLengthSquared(pPfoCluster);
    }

    return lengthSquared;
}

//------------------------------------------------------------------------------------------------------------------------------------------

float LArPfoHelper::GetClosestDistance(const ParticleFlowObject *const pPfo, const Cluster *const pCluster)
{
    const HitType hitType(LArClusterHelper::GetClusterHitType(pCluster));

    ClusterList clusterList;
    LArPfoHelper::GetClusters(pPfo, hitType, clusterList);

    if (clusterList.empty())
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);

    float bestDistance(std::numeric_limits<float>::max());

    for (ClusterList::const_iterator iter = clusterList.begin(), iterEnd = clusterList.end(); iter != iterEnd; ++iter)
    {
        const Cluster* pPfoCluster = *iter;
        const float thisDistance(LArClusterHelper::GetClosestDistance(pCluster, pPfoCluster));

        if (thisDistance < bestDistance)
            bestDistance = thisDistance;
    }

    return bestDistance;
}

//------------------------------------------------------------------------------------------------------------------------------------------

float LArPfoHelper::GetTwoDSeparation(const ParticleFlowObject *const pPfo1, const ParticleFlowObject *const pPfo2)
{
    ClusterList clusterListU1, clusterListV1, clusterListW1;
    ClusterList clusterListU2, clusterListV2, clusterListW2;

    LArPfoHelper::GetClusters(pPfo1, TPC_VIEW_U, clusterListU1);
    LArPfoHelper::GetClusters(pPfo1, TPC_VIEW_V, clusterListV1);
    LArPfoHelper::GetClusters(pPfo1, TPC_VIEW_W, clusterListW1);

    LArPfoHelper::GetClusters(pPfo2, TPC_VIEW_U, clusterListU2);
    LArPfoHelper::GetClusters(pPfo2, TPC_VIEW_V, clusterListV2);
    LArPfoHelper::GetClusters(pPfo2, TPC_VIEW_W, clusterListW2);

    float numViews(0.f);
    float distanceSquared(0.f);

    if (!clusterListU1.empty() && !clusterListU2.empty())
    {
        distanceSquared += LArClusterHelper::GetClosestDistance(clusterListU1, clusterListU2);
        numViews += 1.f;
    }

    if (!clusterListV1.empty() && !clusterListV2.empty())
    {
        distanceSquared += LArClusterHelper::GetClosestDistance(clusterListV1, clusterListV2);
        numViews += 1.f;
    }

    if (!clusterListW1.empty() && !clusterListW2.empty())
    {
        distanceSquared += LArClusterHelper::GetClosestDistance(clusterListW1, clusterListW2);
        numViews += 1.f;
    }

    if (numViews < std::numeric_limits<float>::epsilon())
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);

    return std::sqrt(distanceSquared / numViews);
}

//------------------------------------------------------------------------------------------------------------------------------------------

float LArPfoHelper::GetThreeDSeparation(const ParticleFlowObject *const pPfo1, const ParticleFlowObject *const pPfo2)
{
    ClusterList clusterList1, clusterList2;

    LArPfoHelper::GetClusters(pPfo1, TPC_3D, clusterList1);
    LArPfoHelper::GetClusters(pPfo2, TPC_3D, clusterList2);

    if (clusterList1.empty() || clusterList2.empty())
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);

    return LArClusterHelper::GetClosestDistance(clusterList1, clusterList2);
}

//------------------------------------------------------------------------------------------------------------------------------------------

void LArPfoHelper::GetSlidingFitTrajectory(const ParticleFlowObject *const pPfo, const unsigned int layerWindow, const float layerPitch,
    std::vector<TrackState> &trackStateVector)
{
    // TODO: typedef std::vector<pandora::TrackState> TrackStateVector

    // Require that Pfo has a reconstructed direction
    if (pPfo->GetMomentum().GetMagnitudeSquared() < std::numeric_limits<float>::epsilon())
        throw StatusCodeException(STATUS_CODE_NOT_INITIALIZED);

    const CartesianVector pfoDirection(pPfo->GetMomentum().GetUnitVector());

    // Get 3D clusters (normally there should only be one)
    ClusterList clusterList;
    LArPfoHelper::GetClusters(pPfo, TPC_3D, clusterList);

    if (clusterList.empty())
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);

    // Get 3D vertex (not compulsory, but helps...)
    CartesianVector pfoVertex(0.f, 0.f, 0.f);

    if (!pPfo->GetVertexList().empty())
    {
        if(pPfo->GetVertexList().size() != 1)
            throw StatusCodeException(pandora::STATUS_CODE_FAILURE);

        const Vertex *const pVertex = *(pPfo->GetVertexList().begin());
        pfoVertex = pVertex->GetPosition();
    }

    // Get seed direction from 3D clusters (bail out for single-hit clusters)
    CartesianVector minPosition(0.f, 0.f, 0.f), maxPosition(0.f, 0.f, 0.f);
    LArClusterHelper::GetExtremalCoordinates(clusterList, minPosition, maxPosition);

    if ((maxPosition - minPosition).GetMagnitudeSquared() < std::numeric_limits<float>::epsilon())
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);

    // Ensure that seed direction is aligned with Pfo direction
    const CartesianVector clusterDirection((maxPosition - minPosition).GetUnitVector());
    const float scaleFactor((clusterDirection.GetDotProduct(pfoDirection)) < 0.f ? -1.f : +1.f);

    // Apply 3D sliding linear fits
    typedef std::map< const float, const TrackState > ThreeDTrajectoryMap;
    ThreeDTrajectoryMap trajectoryMap;

    for (ClusterList::const_iterator cIter = clusterList.begin(), cIterEnd = clusterList.end(); cIter != cIterEnd; ++cIter)
    {
        const Cluster *const pCluster = *cIter;

        try
        {
            const ThreeDSlidingFitResult slidingFitResult(pCluster, layerWindow, layerPitch);

            CaloHitList caloHitList;
            pCluster->GetOrderedCaloHitList().GetCaloHitList(caloHitList);

            for (CaloHitList::const_iterator hIter = caloHitList.begin(), hIterEnd = caloHitList.end(); hIter != hIterEnd; ++hIter)
            {
                const CaloHit *const pCaloHit = *hIter;

                try
                {
                    const float rL(slidingFitResult.GetLongitudinalDisplacement(pCaloHit->GetPositionVector()));
                    CartesianVector position(0.f, 0.f, 0.f), direction(0.f, 0.f, 0.f);
                    slidingFitResult.GetGlobalFitPosition(rL, position);
                    slidingFitResult.GetGlobalFitDirection(rL, direction);
                    trajectoryMap.insert(std::pair<const float, const TrackState>(
                        clusterDirection.GetDotProduct(position - pfoVertex) * scaleFactor, TrackState(position, direction * scaleFactor)));
                }
                catch (StatusCodeException &statusCodeException)
                {
                    if (STATUS_CODE_FAILURE == statusCodeException.GetStatusCode())
                        throw statusCodeException;
                }
            }
        }
        catch (StatusCodeException &statusCodeException)
        {
            if (STATUS_CODE_FAILURE == statusCodeException.GetStatusCode())
                throw statusCodeException;
        }
    }

    // Require at least one trajectory point
    if (trajectoryMap.empty())
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);

    // Return trajectory points
    for (ThreeDTrajectoryMap::const_iterator tIter = trajectoryMap.begin(), tIterEnd = trajectoryMap.end(); tIter != tIterEnd; ++tIter)
    {
        const TrackState &nextPoint = tIter->second;
        trackStateVector.push_back(nextPoint);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool LArPfoHelper::IsTrack(const ParticleFlowObject *const pPfo)
{
    const int pdg(pPfo->GetParticleId());

    // muon, pion, proton, kaon
    return ((MU_MINUS == std::abs(pdg)) || (PI_PLUS == std::abs(pdg)) || (PROTON == std::abs(pdg)) || (K_PLUS == std::abs(pdg)));
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool LArPfoHelper::IsShower(const ParticleFlowObject *const pPfo)
{
    const int pdg(pPfo->GetParticleId());

    // electron, photon
    return ((E_MINUS == std::abs(pdg)) || (PHOTON == std::abs(pdg)));
}

//------------------------------------------------------------------------------------------------------------------------------------------

int LArPfoHelper::GetPrimaryNeutrino(const ParticleFlowObject *const pPfo)
{
    try
    {
        const ParticleFlowObject *pParentPfo = LArPfoHelper::GetParentNeutrino(pPfo);
        return pParentPfo->GetParticleId();
    }
    catch (const StatusCodeException &)
    {
        return 0;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool LArPfoHelper::IsFinalState(const ParticleFlowObject *const pPfo)
{
    if (pPfo->GetParentPfoList().size() == 0 && !LArPfoHelper::IsNeutrino(pPfo))
        return true;

    if (LArPfoHelper::IsNeutrinoFinalState(pPfo))
        return true;

    return false;
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool LArPfoHelper::IsNeutrinoFinalState(const ParticleFlowObject *const pPfo)
{
    return ((pPfo->GetParentPfoList().size() == 1) && (LArPfoHelper::IsNeutrino(*(pPfo->GetParentPfoList().begin()))));
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool LArPfoHelper::IsNeutrino(const ParticleFlowObject *const pPfo)
{
    const int absoluteParticleId(std::abs(pPfo->GetParticleId()));

    if ((NU_E == absoluteParticleId) || (NU_MU == absoluteParticleId) || (NU_TAU == absoluteParticleId))
        return true;

    return false;
}

//------------------------------------------------------------------------------------------------------------------------------------------

const ParticleFlowObject *LArPfoHelper::GetParentPfo(const ParticleFlowObject *const pPfo)
{
    const ParticleFlowObject *pParentPfo = pPfo;

    while (pParentPfo->GetParentPfoList().empty() == false)
    {
        pParentPfo = *(pParentPfo->GetParentPfoList().begin());
    }

    return pParentPfo;
}

//------------------------------------------------------------------------------------------------------------------------------------------

const ParticleFlowObject *LArPfoHelper::GetParentNeutrino(const ParticleFlowObject *const pPfo)
{
    const ParticleFlowObject *pParentPfo = LArPfoHelper::GetParentPfo(pPfo);

    if(!LArPfoHelper::IsNeutrino(pParentPfo))
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);

    return pParentPfo;
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool LArPfoHelper::SortByNHits(const ParticleFlowObject *const pLhs, const ParticleFlowObject *const pRhs)
{
    unsigned int nHitsLhs(0);
    for (ClusterList::const_iterator iter = pLhs->GetClusterList().begin(), iterEnd = pLhs->GetClusterList().end(); iter != iterEnd; ++iter)
    {
        const Cluster *pClusterLhs = *iter;

        if (TPC_3D != LArClusterHelper::GetClusterHitType(pClusterLhs))
            continue;

        nHitsLhs += pClusterLhs->GetNCaloHits();
    }

    unsigned int nHitsRhs(0);
    for (ClusterList::const_iterator iter = pRhs->GetClusterList().begin(), iterEnd = pRhs->GetClusterList().end(); iter != iterEnd; ++iter)
    {
        const Cluster *pClusterRhs = *iter;

        if (TPC_3D != LArClusterHelper::GetClusterHitType(pClusterRhs))
            continue;

        nHitsRhs += pClusterRhs->GetNCaloHits();
    }

    if (nHitsLhs != nHitsRhs)
        return (nHitsLhs > nHitsRhs);

    return (LArPfoHelper::GetTwoDLengthSquared(pLhs) > LArPfoHelper::GetTwoDLengthSquared(pRhs));
}

} // namespace lar_content
