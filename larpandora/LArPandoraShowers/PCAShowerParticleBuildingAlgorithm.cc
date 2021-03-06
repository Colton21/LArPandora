/**
 *  @file   larpandora/LArPandoraShowers/PCAShowerParticleBuildingAlgorithm.cc
 *
 *  @brief  Implementation of the 3D shower building algorithm class.
 *
 *  $Log: $
 */

#include "Pandora/AlgorithmHeaders.h"

#include "larpandoracontent/LArHelpers/LArClusterHelper.h"
#include "larpandoracontent/LArHelpers/LArGeometryHelper.h"
#include "larpandoracontent/LArHelpers/LArPfoHelper.h"

#include "larpandoracontent/LArObjects/LArShowerPfo.h"
#include "larpandoracontent/LArObjects/LArThreeDSlidingFitResult.h"

#include "larpandora/LArPandoraShowers/PCAShowerParticleBuildingAlgorithm.h"

#include <Eigen/Dense>

using namespace pandora;
using namespace lar_content;

namespace lar_pandora_showers
{

PCAShowerParticleBuildingAlgorithm::PCAShowerParticleBuildingAlgorithm() :
    m_cosmicMode(false),
    m_layerFitHalfWindow(20)
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PCAShowerParticleBuildingAlgorithm::CreatePfo(const ParticleFlowObject *const pInputPfo, const ParticleFlowObject*& pOutputPfo) const
{
    try
    {
        // Need an input vertex to provide a shower propagation direction
        const Vertex *const pInputVertex = LArPfoHelper::GetVertex(pInputPfo);

        // In cosmic mode, build showers from all daughter pfos, otherwise require that pfo is shower-like
        if (m_cosmicMode)
        {
            if (LArPfoHelper::IsFinalState(pInputPfo))
                return;
        }
        else
        {
            if (!LArPfoHelper::IsShower(pInputPfo))
                return;
        }

        // Build a new pfo
        LArShowerPfoFactory pfoFactory;
        LArShowerPfoParameters pfoParameters;
        pfoParameters.m_particleId = (LArPfoHelper::IsShower(pInputPfo) ? pInputPfo->GetParticleId() : E_MINUS);
        pfoParameters.m_charge = PdgTable::GetParticleCharge(pfoParameters.m_particleId.Get());
        pfoParameters.m_mass = PdgTable::GetParticleMass(pfoParameters.m_particleId.Get());
        pfoParameters.m_energy = 0.f;
        pfoParameters.m_momentum = pInputPfo->GetMomentum();
        pfoParameters.m_showerVertex = pInputVertex->GetPosition();

        ClusterList threeDClusterList;
        LArPfoHelper::GetThreeDClusterList(pInputPfo, threeDClusterList);

        if (!threeDClusterList.empty())
        {
            CaloHitList threeDCaloHitList; // Might be able to get through LArPfoHelper instead
            const Cluster *const pThreeDCluster(threeDClusterList.front());
            pThreeDCluster->GetOrderedCaloHitList().FillCaloHitList(threeDCaloHitList);

            EigenVectors eigenVecs;
            CartesianVector centroid(0.f, 0.f, 0.f), eigenValues(0.f, 0.f, 0.f);
            this->RunPCA(threeDCaloHitList, centroid, eigenValues, eigenVecs);

            try
            {
                pfoParameters.m_showerCentroid = centroid;
                pfoParameters.m_showerDirection = eigenVecs.at(0);
                pfoParameters.m_showerSecondaryVector = eigenVecs.at(1);
                pfoParameters.m_showerTertiaryVector = eigenVecs.at(2);
                pfoParameters.m_showerEigenValues = eigenValues;
                pfoParameters.m_showerLength = this->ShowerLength(pfoParameters.m_showerEigenValues.Get());
                pfoParameters.m_showerOpeningAngle = this->OpeningAngle(pfoParameters.m_showerDirection.Get(),
                    pfoParameters.m_showerSecondaryVector.Get(), pfoParameters.m_showerEigenValues.Get());
            }
            catch (const StatusCodeException &statusCodeException)
            {
                if (STATUS_CODE_FAILURE == statusCodeException.GetStatusCode())
                    throw statusCodeException;
            }

            try
            {
                const float layerPitch(LArGeometryHelper::GetWireZPitch(this->GetPandora()));
                const ThreeDSlidingFitResult threeDFitResult(pThreeDCluster, m_layerFitHalfWindow, layerPitch);
                pfoParameters.m_showerMinLayerPosition = threeDFitResult.GetGlobalMinLayerPosition();
                pfoParameters.m_showerMaxLayerPosition = threeDFitResult.GetGlobalMaxLayerPosition();
            }
            catch (const StatusCodeException &statusCodeException)
            {
                if (STATUS_CODE_FAILURE == statusCodeException.GetStatusCode())
                    throw statusCodeException;
            }
        }

        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::ParticleFlowObject::Create(*this, pfoParameters, pOutputPfo,
            pfoFactory));

        const LArShowerPfo *const pLArPfo = dynamic_cast<const LArShowerPfo*>(pOutputPfo);

        if (!pLArPfo)
            throw StatusCodeException(STATUS_CODE_FAILURE);

        // Build a new vertex
        const Vertex *pOutputVertex = nullptr;

        PandoraContentApi::Vertex::Parameters vtxParameters;
        vtxParameters.m_position = pInputVertex->GetPosition();
        vtxParameters.m_vertexLabel = pInputVertex->GetVertexLabel();
        vtxParameters.m_vertexType = pInputVertex->GetVertexType();

        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::Vertex::Create(*this, vtxParameters, pOutputVertex));
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::AddToPfo<Vertex>(*this, pOutputPfo, pOutputVertex));
    }
    catch (StatusCodeException &statusCodeException)
    {
        if (STATUS_CODE_FAILURE == statusCodeException.GetStatusCode())
            throw statusCodeException;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PCAShowerParticleBuildingAlgorithm::RunPCA(const pandora::CaloHitList &threeDCaloHitList, pandora::CartesianVector &centroid,
    pandora::CartesianVector &outputEigenValues, EigenVectors &outputEigenVecs) const
{
    // The steps are:
    // 1) do a mean normalization of the input vec points
    // 2) compute the covariance matrix
    // 3) run the SVD
    // 4) extract the eigen vectors and values

    // Run through the CaloHitList and get the mean position of all the hits
    double meanPosition[3] = {0., 0., 0.};
    unsigned int nThreeDHits(0);

    for (const CaloHit *const pCaloHit3D : threeDCaloHitList)
    {
        meanPosition[0] += pCaloHit3D->GetPositionVector().GetX();
        meanPosition[1] += pCaloHit3D->GetPositionVector().GetY();
        meanPosition[2] += pCaloHit3D->GetPositionVector().GetZ();
        ++nThreeDHits;
    }

    if (0 == nThreeDHits)
    {
        std::cout << "PCAShowerParticleBuildingAlgorithm::RunPCA - No three dimensional hit found!" << std::endl;
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);
    }

    const double nThreeDHitsAsDouble(static_cast<double>(nThreeDHits));
    meanPosition[0] /= nThreeDHitsAsDouble;
    meanPosition[1] /= nThreeDHitsAsDouble;
    meanPosition[2] /= nThreeDHitsAsDouble;
    centroid = CartesianVector(meanPosition[0], meanPosition[1], meanPosition[2]);

    // Define elements of our covariance matrix
    double xi2(0.);
    double xiyi(0.);
    double xizi(0.);
    double yi2(0.);
    double yizi(0.);
    double zi2(0.);
    double weightSum(0.);

    for (const CaloHit *const pCaloHit3D : threeDCaloHitList)
    {
        const double weight(1.);
        const CartesianVector &hitPosition(pCaloHit3D->GetPositionVector());
        const double x((hitPosition.GetX() - meanPosition[0]) * weight);
        const double y((hitPosition.GetY() - meanPosition[1]) * weight);
        const double z((hitPosition.GetZ() - meanPosition[2]) * weight);

        xi2  += x * x;
        xiyi += x * y;
        xizi += x * z;
        yi2  += y * y;
        yizi += y * z;
        zi2  += z * z;
        weightSum += weight * weight;
    }

    // Using Eigen package
    Eigen::Matrix3f sig;

    sig <<  xi2, xiyi, xizi,
           xiyi,  yi2, yizi,
           xizi, yizi,  zi2;

    if (std::fabs(weightSum) < std::numeric_limits<double>::epsilon())
    {
        std::cout << "PCAShowerParticleBuildingAlgorithm::RunPCA - The total weight of three dimensional hits is 0!" << std::endl;
        throw StatusCodeException(STATUS_CODE_INVALID_PARAMETER);
    }

    sig *= 1. / weightSum;

    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> eigenMat(sig);

    if (eigenMat.info() != Eigen::ComputationInfo::Success)
    {
        std::cout << "PCAShowerParticleBuildingAlgorithm::RunPCA - PCA decompose failure, number of three D hits = " << nThreeDHits << std::endl;
        throw StatusCodeException(STATUS_CODE_INVALID_PARAMETER);
    }

    typedef std::pair<float,size_t> EigenValColPair;
    typedef std::vector<EigenValColPair> EigenValColVector;

    EigenValColVector eigenValColVector;
    const auto &resultEigenMat(eigenMat.eigenvalues());
    eigenValColVector.emplace_back(resultEigenMat(0), 0);
    eigenValColVector.emplace_back(resultEigenMat(1), 1);
    eigenValColVector.emplace_back(resultEigenMat(2), 2);

    std::sort(eigenValColVector.begin(), eigenValColVector.end(), [](const EigenValColPair &left, const EigenValColPair &right){return left.first > right.first;} );

    // Now copy output
    // Get the eigen values
    outputEigenValues = CartesianVector(eigenValColVector.at(0).first, eigenValColVector.at(1).first, eigenValColVector.at(2).first);

    // Grab the principle axes
    const Eigen::Matrix3f &eigenVecs(eigenMat.eigenvectors());

    for (const EigenValColPair &pair : eigenValColVector)
    {
        outputEigenVecs.emplace_back(eigenVecs(0, pair.second), eigenVecs(1, pair.second), eigenVecs(2, pair.second));
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

CartesianVector PCAShowerParticleBuildingAlgorithm::ShowerLength(const CartesianVector &eigenValues) const
{
    float sl[3] = {0.f, 0.f, 0.f};

    if (eigenValues.GetX() > 0.f)
    {
        sl[0] = 6.f * std::sqrt(eigenValues.GetX());
    }
    else
    {
        std::cout << "The principal eigenvalue is equal to or less than 0." << std::endl;
        throw StatusCodeException( STATUS_CODE_INVALID_PARAMETER );
    }

    if (eigenValues.GetY() > 0.f)
        sl[1] = 6.f * std::sqrt(eigenValues.GetY());

    if (eigenValues.GetZ() > 0.f)
        sl[2] = 6.f * std::sqrt(eigenValues.GetZ());

    return CartesianVector(sl[0], sl[1], sl[2]);
}

//------------------------------------------------------------------------------------------------------------------------------------------

float PCAShowerParticleBuildingAlgorithm::OpeningAngle(const CartesianVector &principal, const CartesianVector &secondary,
    const CartesianVector &eigenValues) const
{
    const float principalMagnitude(principal.GetMagnitude());
    const float secondaryMagnitude(secondary.GetMagnitude());

    if (std::fabs(principalMagnitude) < std::numeric_limits<float>::epsilon())
    {
        std::cout << "PCAShowerParticleBuildingAlgorithm::OpeningAngle - The principal eigenvector is 0." << std::endl;
        throw StatusCodeException(STATUS_CODE_INVALID_PARAMETER);
    }
    else if (std::fabs(secondaryMagnitude) < std::numeric_limits<float>::epsilon())
    {
        return 0.f;
    }

    const float cosTheta(principal.GetDotProduct(secondary) / (principalMagnitude * secondaryMagnitude));

    if (cosTheta > 1.f)
    {
        std::cout << "PCAShowerParticleBuildingAlgorithm::OpeningAngle - cos(theta) reportedly greater than 1." << std::endl;
        throw StatusCodeException(STATUS_CODE_INVALID_PARAMETER);
    }

    const float sinTheta(std::sqrt(1.f - cosTheta * cosTheta));

    if (std::fabs(eigenValues.GetX()) < std::numeric_limits<float>::epsilon())
    {
        std::cout << "PCAShowerParticleBuildingAlgorithm::OpeningAngle - principal eigenvalue less than or equal to 0." << std::endl;
        throw StatusCodeException( STATUS_CODE_INVALID_PARAMETER );
    }
    else if (std::fabs(eigenValues.GetY()) < std::numeric_limits<float>::epsilon())
    {
        return 0.f;
    }

    return std::atan(std::sqrt(eigenValues.GetY()) * sinTheta / std::sqrt(eigenValues.GetX()));
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode PCAShowerParticleBuildingAlgorithm::ReadSettings(const TiXmlHandle xmlHandle)
{
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "CosmicMode", m_cosmicMode));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "LayerFitHalfWindow", m_layerFitHalfWindow));

    return CustomParticleCreationAlgorithm::ReadSettings(xmlHandle);
}

} // namespace lar_pandora_showers
