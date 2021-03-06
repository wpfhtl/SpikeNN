#include "Layer.h"
#include "Network.h"
#include <stdlib.h>
#include <iostream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>

#define RANDOMINPUT ((float)rand()/RAND_MAX)*(mMaxInputCurrent - mMinInputCurrent) + mMinInputCurrent

Layer::Layer(Network* net, int ID, bool shouldLearn, bool isContainer)
{
   initialize();
   mNetwork = net;
   setExcitatoryLearningFlag(shouldLearn); setInhibitoryLearningFlag(shouldLearn);
   setContainerFlag(isContainer);
   mID = ID;
   wakeup();
}

void Layer::initialize()
{
   mDAHandler = 0;
   mTime = 0;
   mTimeStep = 0;
   mInputPatternMode = NO_INPUT;
   mInputPattern = 0;
   mLockExLearningFlag = mLockInLearningFlag = false;
   mLogActivityFlag = false;
   mAP = 0.1f;
   mAN = 0.12f;
   mTaoP = 20.0f;
   mTaoN = 20.0f;
   mCMultiplier = 0.9f;
   mDecayWeightMultiplier = 1.0f;
   mSTDPTimeStep = 100;
   mExMaxWeight = 10;
   mExMaxRandWeight = 10;
   mExMinRandWeight = 0;
   mInMaxWeight = 20;
   mInMaxRandWeight = 20;
   mInMinRandWeight = 0;
   mMaxRandDelay = 20;
   mMinRandDelay = 0;
   mMinInputCurrent = 0;
   mMaxInputCurrent = 20;
   mSharedConnectionFlag = false;
   mSharedConnectionTimeStep = -1;
   mSharedWinnerID = -1;
}

void Layer::wakeup()
{
   mTime = mNetwork->getPointerToTime();
   mTimeStep = mNetwork->getPointerToTimeStep();
   updateLearningFlags();

   for (size_t i = 0; i < mNeurons.size(); ++i)
      mNeurons[i]->wakeup();

   if (mDAHandler)
      mDAHandler->wakeup();
}

Layer::~Layer()
{
   if (mLogActivityFlag)
      flushActivity();
   
   for (std::size_t i = 0; i < mNeurons.size(); ++i) 
      delete mNeurons[i];

   for (std::size_t i = 0; i < mSharedConnections.size(); ++i) 
      delete mSharedConnections[i];

   if(mDAHandler)
      delete mDAHandler;
}

void Layer::update()
{
   if (mDAHandler)
      mDAHandler->update();

   size_t k;

   if (std::fmod(*mTime, 1) < *mTimeStep)
   switch (mInputPatternMode)
   {
   case NO_INPUT:
      break;

   case ALL_RANDOM_CURRENT:
      for (k = 0; k < mNeurons.size(); k++)
         mNeurons[k]->addInputCurrent(*mTime, RANDOMINPUT);
      break;

   case ALL_MAX_CURRENT:
      for (k = 0; k < mNeurons.size(); k++)
         mNeurons[k]->addInputCurrent(*mTime, RANDOMINPUT);
      break;

   case ONE_RANDOM_CURRENT:
      k = rand() % mNeurons.size();
      mNeurons[k]->addInputCurrent(*mTime, RANDOMINPUT);
      break;

   case ONE_MAX_CURRENT:
      k = rand() % mNeurons.size();
      mNeurons[k]->addInputCurrent(*mTime, mMaxInputCurrent);
      break;

   case MANUAL_INPUT:
      {
      std::vector<InputInformation> infos = (mInputPattern) ? 
         (*mInputPattern)(*mTime) :
         mNetwork->defaultInputPattern(*mTime);

      for (size_t i = 0; i < infos.size(); ++i)
      {
         switch (infos[i].mInputMode)
         {
         case FORCE_FIRE:
            mNeurons[infos[i].mNeuronIndex]->propagateSpike();
            break;

         case RANDOM_CURRENT:
            mNeurons[infos[i].mNeuronIndex]->addInputCurrent(*mTime, RANDOMINPUT);
            break;

         case MAX_CURRENT:
            mNeurons[infos[i].mNeuronIndex]->addInputCurrent(*mTime, mMaxInputCurrent);
            break;

         case MANUAL_CURRENT:
            mNeurons[infos[i].mNeuronIndex]->addInputCurrent(*mTime, infos[i].mManualCurrent);
            break;

         default:
            break;
         }
      }
      break;
      }
   default:
      break;
   }

   if (!mContainerFlag)
   {
      //call neurons to update in a random order??
      //std::vector<std::size_t> order = SHUFFLE(mNeurons.size());
      for (size_t i = 0; i < mNeurons.size(); ++i)
         mNeurons[i]->update();
   }

   if (mSharedConnectionFlag)
      if (fmod(*mTime, mSharedConnectionTimeStep) < *mTimeStep)
      {
         mSharedWinnerID = -1;
      }

   if (mExShouldLearn || mInShouldLearn)
      if (fmod(*mTime, mSTDPTimeStep) < *mTimeStep)
      {
         if (mSharedConnectionFlag)
         {
            for (size_t i = 0; i < mSharedConnections.size(); ++i)
            {
               if ((mSharedConnections[i]->getType() == EXCITATORY && mExShouldLearn) ||
                  (mSharedConnections[i]->getType() == INHIBITORY && mInShouldLearn))
                  mSharedConnections[i]->updateWeight();
            }
         }
         else
         {
            for (size_t i = 0; i < mSynapses.size(); ++i)
            {
               if ((mSynapses[i]->getType() == EXCITATORY && mExShouldLearn) ||
                  (mSynapses[i]->getType() == INHIBITORY && mInShouldLearn))
                  mSynapses[i]->mBase->updateWeight();
            }
         }

         for (size_t i = 0; i < mSynapsesToLog.size(); ++i)
         {
            mSynapsesToLog[i]->logCurrentWeight();
         }
      }

   if (mLogActivityFlag)
      if (fmod(*mTime, 60000) < *mTimeStep)
         flushActivity();
}

void Layer::flushActivity()
{
   int min = (int)std::floor(*mTime / 60000);
   int startmin = (min-1) * 60000;
   mLogger.set("Layer" + Logger::toString((float)mID) + "Min" + Logger::toString((float)(min)));

   std::string s = "";
   for (size_t i = 0; i < mSpikes.size(); ++i)
      s += (Logger::toString((float)mSpikes[i].mTime - startmin) + " " + Logger::toString((float)mSpikes[i].mNeuronID) + "\n");

   mSpikes.clear();

   if(s != "")
      mLogger.write(s);
}


void Layer::makeConnection(Layer* source, Layer* dest, float synapseProb, float excitatoryWeight,
      float inhibitoryWeight, int excitatoryDelay, int inhibitoryDelay)
{
   //TODO: throw error if the two layers dosn't belong to a same network
   
   for (std::size_t i = 0; i < source->mNeurons.size(); ++i)
      for (std::size_t j = 0; j < dest->mNeurons.size(); ++j)
      {
         float f = float(rand()) / RAND_MAX;
         if (f < synapseProb)
         {
            if (source->mNeurons[i]->getType() == EXCITATORY)
            {
               Synapse* syn = Neuron::makeConnection(source->mNeurons[i], dest->mNeurons[j], excitatoryWeight, excitatoryDelay);
               dest->mSynapses.push_back(syn);
               //if (source != dest)
               //   dest->mSynapses.push_back(syn);
            }
            else
            {
               Synapse* syn = Neuron::makeConnection(source->mNeurons[i], dest->mNeurons[j], inhibitoryWeight, inhibitoryDelay);
               dest->mSynapses.push_back(syn);
               //if (source != dest)
               //   dest->mSynapses.push_back(syn);
            }
         }
      }
}

void Layer::makeConnection(Layer* source, Layer* dest,
      int neuronsNumToConnect, float excitatoryWeight, float inhibitoryWeight,
      int excitatoryDelay, int inhibitoryDelay)
{
   for (size_t i = 0; i < source->mNeurons.size(); ++i)
   {
      for (int j = 0; j < neuronsNumToConnect; ++j)
      {
         int k = rand() % dest->mNeurons.size();

         //no self or multiple or inhibitory to inhibitory connection!
         if (i == k || source->mNeurons[i]->isConnectedTo(dest->mNeurons[k])
            || (source->mNeurons[i]->getType() == INHIBITORY && dest->mNeurons[k]->getType() == INHIBITORY))
         {
            --j;
            continue;
         }

         if (source->mNeurons[i]->getType() == EXCITATORY)
         {
            Synapse* syn = Neuron::makeConnection(source->mNeurons[i], dest->mNeurons[k], excitatoryWeight, excitatoryDelay);
            dest->mSynapses.push_back(syn);
            //if (source != dest)
            //   dest->mSynapses.push_back(syn);
         }
         else
         {
            Synapse* syn = Neuron::makeConnection(source->mNeurons[i], dest->mNeurons[k], inhibitoryWeight, inhibitoryDelay);
            dest->mSynapses.push_back(syn);
            //if (source != dest)
            //   dest->mSynapses.push_back(syn);
         }
      }
   }
}

void Layer::makeConnection(Layer* source, Layer* dest, ConnectionInfo (*pattern)(int, int))
{
   for (size_t i = 0; i < source->mNeurons.size(); ++i)
      for (size_t j = 0; j < dest->mNeurons.size(); ++j)
      {
         ConnectionInfo info = (pattern) ? (*pattern)(i, j) : source->mNetwork->defaultConnectingPattern(i, j);
         if (info.mConnectFlag)
         {
            Synapse* syn = Neuron::makeConnection(source->mNeurons[i], dest->mNeurons[j], info.mWeight, info.mDelay, info.mType);
            dest->mSynapses.push_back(syn);

            //is it right??
            //if (source != dest)
            //   dest->mSynapses.push_back(syn);
         }
      }
}

void Layer::logWeight(bool (*pattern)(int))
{
   for (size_t i = 0; i < mSynapses.size(); ++i)
   {
      if ((*pattern)(mSynapses[i]->getID()))
         mSynapsesToLog.push_back(mSynapses[i]);
   }
}

//
//void Layer::logWeight(bool (*pattern)(int, int, int, int))
//{
//   for (size_t i = 0; i < mSynapses.size(); ++i)
//   {
//      mSynapses[i]->logWeight(pattern);
//   }
//}

void Layer::logPotential(bool (*pattern)(int))
{
   for (size_t i = 0; i < mNeurons.size(); ++i)
   {
      if(pattern)
         if(!(*pattern)(i))
            continue;

      mNeurons[i]->logPotential();
   }
}

//void Layer::logPostSynapseWeight(int neuron, std::string directory)
//{
//   mNeurons[neuron]->logPostSynapseWeight(directory);
//}
//
//void Layer::logPreSynapseWeight(int neuron, std::string directory)
//{
//   mNeurons[neuron]->logPreSynapseWeight(directory);
//}

//std::vector<int> Layer::getWeightFrequencies()
//{
//   std::vector<int> freq ((unsigned int)(mExMaxWeight / 0.2)+1, 0);
//   for (size_t i = 0; i < mSynapses.size(); ++i)
//      freq[(unsigned int)(mSynapses[i]->getWeight() / 0.2)]++;
//
//   return freq;
//}

void Layer::recordSpike(int NeuronID)
{
   if (mLogActivityFlag)
   {
      SpikeInfo s = {NeuronID, mNetwork->getTime()};
      mSpikes.push_back(s);
   }
   
   if (mDAHandler)
      mDAHandler->notifyOfSpike(NeuronID);
}

const Synapse* Layer::getSynapse(int synapseID)
{
   for (size_t i = 0; i < mSynapses.size(); ++i)
   {
      if (mSynapses[i]->getID() == synapseID)
         return mSynapses[i];
   }

   return 0;
}

int Layer::getNextSynapseID()
{
   return mNetwork->getNextSynapseID();
}

void Layer::setBoundingParameters(float exMaxWeight, float inMaxWeight, float exMinRandWeight, 
      float exMaxRandWeight, float inMinRandWeight, float inMaxRandWeight,
      int minRandDelay, int maxRandDelay)
{
   if (exMaxWeight != -1) mExMaxWeight = exMaxWeight;
   if (exMaxRandWeight != -1) mExMaxRandWeight = exMaxRandWeight;
   if (exMinRandWeight != -1) mExMinRandWeight = exMinRandWeight;
   if (inMaxWeight != -1) mInMaxWeight = inMaxWeight;
   if (inMaxRandWeight != -1) mInMaxRandWeight = inMaxRandWeight;
   if (inMinRandWeight != -1) mInMinRandWeight = inMinRandWeight;
   if (maxRandDelay != -1) mMaxRandDelay = maxRandDelay;
   if (minRandDelay != -1) mMinRandDelay = minRandDelay;
}

void Layer::restNeurons()
{
   for (size_t i = 0; i < mNeurons.size(); ++i)
      mNeurons[i]->rest();
}

std::string Layer::getAddress(int slayer, int sneuron, int dlayer, int dneuron)
{
   return mNetwork->getAddress(slayer, sneuron, dlayer, dneuron);
}

void Layer::shareConnection(size_t sourceNeuron, int sharingTimeStep)
{
   std::vector<SynapseBase*> bases = mNeurons[sourceNeuron]->getPreSynapsesToShare();

   for (size_t i=0; i<mNeurons.size(); ++i)
      if (i != sourceNeuron)
         mNeurons[i]->setPreSynapsesToShare(bases);

   mSharedConnectionFlag = true;
   mSharedConnections = bases;
   mSharedConnectionTimeStep = sharingTimeStep;
   mSynapses.clear();
}

void Layer::shareConnection(std::vector<SynapseBase*> bases, int sharingTimeStep)
{
   for (size_t i=0; i<mNeurons.size(); ++i)
         mNeurons[i]->setPreSynapsesToShare(bases);

   for (size_t i=0; i<mSharedConnections.size(); ++i)
      delete mSharedConnections[i];

   mSharedConnectionFlag = true;
   mSharedConnections = bases;
   mSharedConnectionTimeStep = sharingTimeStep;
   mSynapses.clear();
}

//void Layer::giveChangeRequest(STDPchangeRequest request)
//{
//   //record the request and also sort the list using insertion sort
//   //mCChangeRequests.push_back(request);
//   int i = mCChangeRequests.size()-1;
//   for (; i>=0; --i)
//      if (mCChangeRequests[i].mNeuronID == request.mNeuronID)
//         break;
//
//   mCChangeRequests.insert(mCChangeRequests.begin() + (i + 1), request);
//}

template <class Archive>
void Layer::serialize(Archive &ar, const unsigned int version)
{
   ar & mNetwork & mID
      & mNeurons & mSynapses
      & mInputPatternMode & mExLearningFlag
      & mInLearningFlag & mLockExLearningFlag
      & mLockInLearningFlag & mContainerFlag
      & mAP & mAN
      & mTaoP & mTaoN
      & mCMultiplier & mDecayWeightMultiplier
      & mSTDPTimeStep & mExMaxWeight 
      & mExMaxRandWeight & mExMinRandWeight
      & mInMaxWeight & mInMaxRandWeight
      & mInMinRandWeight & mMaxRandDelay
      & mMinRandDelay & mMinInputCurrent
      & mMaxInputCurrent & mSharedConnectionFlag
      & mSharedConnections & mSharedConnectionTimeStep
      & mDAHandler;
}

template void Layer::serialize<boost::archive::text_oarchive>(boost::archive::text_oarchive &ar, const unsigned int version);
template void Layer::serialize<boost::archive::text_iarchive>(boost::archive::text_iarchive &ar, const unsigned int version);
